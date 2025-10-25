// ESP32-S3 USB Host bridge for TI-84 Plus CE Python (Clean)
// Device path: Seeed Studio XIAO ESP32S3 in USB Host mode
// Bridge: TI-84 (USB device) <-> ESP32-S3 (USB Host + WiFi) <-> Node server

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <UrlEncode.h>

#include "../esp32/secrets.h"

#if !CONFIG_IDF_TARGET_ESP32S3
#  error "This sketch must be built for ESP32-S3 (USB Host)"
#endif

extern "C" {
  #include "usb/usb_host.h"
  #include "usb/usb_types_ch9.h"
}

// Hardware: VBUS high-side switch enable pin for XIAO ESP32S3
#ifndef PIN_VBUS_EN
#define PIN_VBUS_EN 10 // GPIO10 = D16 on XIAO ESP32S3
#endif

// TI USB IDs
static const uint16_t TI_VID = 0x0451;
static const uint16_t TI_84_PID = 0xE008; // TI-84 Plus CE Python

// Simplified TI Link protocol constants (placeholder)
static const uint8_t TI_CMD_SCR = 0x06;  // Store variable
static const uint8_t TI_CMD_REQ = 0x87;  // Request variable
static const uint8_t TI_VAR_STR = 0x15;  // String

// Global state
static bool wifiReady = false;
static bool usbHostReady = false;
static bool calculatorConnected = false;
static bool vbusEnabled = false;

static usb_host_client_handle_t g_usb_client = nullptr;
static usb_host_device_handle_t g_dev = nullptr;
static const usb_config_desc_t* g_cfg = nullptr;
static uint8_t g_dev_addr = 0;
static uint8_t g_ep_in = 0, g_ep_out = 0;

static bool wifi_connect();
static bool http_get(const String& url, String& out);
bool init_usb_host();
bool wait_for_calculator();
bool open_calculator_pipes();
bool get_string_variable(uint8_t index, String& out);
bool set_string_variable(uint8_t index, const String& val);
void handle_usb_events();

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[S3 Host] Deepseek_Ti84 bridge starting...");

  wifiReady = wifi_connect();
  Serial.printf("WiFi: %s\n", wifiReady ? "connected" : "failed");

  pinMode(PIN_VBUS_EN, OUTPUT);
  digitalWrite(PIN_VBUS_EN, LOW);

  usbHostReady = init_usb_host();
  Serial.printf("USB Host: %s\n", usbHostReady ? "ready" : "failed");

  if (wifiReady && usbHostReady) {
    digitalWrite(PIN_VBUS_EN, HIGH);
    vbusEnabled = true;
    Serial.printf("VBUS enabled on GPIO%d\n", PIN_VBUS_EN);
  }
}

void loop() {
  static bool pipes_open = false;
  static unsigned long last_poll = 0;

  if (usbHostReady && !calculatorConnected) {
    calculatorConnected = wait_for_calculator();
    if (calculatorConnected) {
      Serial.println("Calculator detected, opening pipes...");
      pipes_open = open_calculator_pipes();
      if (pipes_open) {
        Serial.println("Pipes open. Protocol: Str1=question; Str0=GO; answer->Str2; Str0=DONE");
      } else {
        Serial.println("Failed to open pipes");
        calculatorConnected = false;
      }
    }
  }

  if (calculatorConnected && pipes_open && wifiReady && millis() - last_poll > 500) {
    last_poll = millis();

    String trigger;
    if (get_string_variable(0, trigger)) {
      trigger.trim();
      if (trigger == "GO" || trigger == "ASK") {
        Serial.println("Trigger received -> reading Str1...");
        String question;
        if (get_string_variable(1, question)) {
          question.trim();
          if (question.length()) {
            set_string_variable(0, "WAIT");
            String url = String(SERVER) + "/gpt/ask?question=" + urlEncode(question);
            String answer;
            Serial.printf("Querying server: %s\n", url.c_str());
            if (http_get(url, answer)) {
              if (answer.length() > 250) answer = answer.substring(0, 247) + "...";
              if (set_string_variable(2, answer)) {
                set_string_variable(0, "DONE");
                Serial.println("Answer sent.");
              } else {
                set_string_variable(0, "ERROR");
              }
            } else {
              set_string_variable(2, "Network error");
              set_string_variable(0, "ERROR");
            }
          }
        }
      }
    }

    handle_usb_events();
  }

  delay(5);
}

// USB Host setup
bool init_usb_host() {
  esp_err_t err;
  usb_host_config_t host_config = { .intr_flags = ESP_INTR_FLAG_LEVEL1 };
  err = usb_host_install(&host_config);
  if (err != ESP_OK) {
    Serial.printf("usb_host_install failed: %s\n", esp_err_to_name(err));
    return false;
  }

  usb_host_client_config_t client_config = {
    .is_synchronous = false,
    .max_num_event_msg = 16,
    .async = { .client_event_callback = nullptr, .callback_arg = nullptr }
  };
  err = usb_host_client_register(&client_config, &g_usb_client);
  if (err != ESP_OK) {
    Serial.printf("usb_host_client_register failed: %s\n", esp_err_to_name(err));
    return false;
  }

  return true;
}

bool wait_for_calculator() {
  if (!g_usb_client) return false;
  usb_host_client_event_msg_t msg;
  if (usb_host_client_receive(g_usb_client, &msg, 0) == ESP_OK) {
    if (msg.event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
      g_dev_addr = msg.new_dev.address;
      Serial.printf("New USB device @%d\n", g_dev_addr);
      if (usb_host_device_open(g_usb_client, g_dev_addr, &g_dev) == ESP_OK) {
        usb_device_desc_t dev_desc;
        if (usb_host_get_device_desc(g_dev, &dev_desc) == ESP_OK &&
            dev_desc.idVendor == TI_VID && dev_desc.idProduct == TI_84_PID) {
          usb_host_get_active_config_desc(g_dev, &g_cfg);
          Serial.println("TI-84 detected");
          return true;
        } else {
          Serial.println("Not TI-84, closing");
          usb_host_device_close(g_usb_client, g_dev);
          g_dev = nullptr;
        }
      }
    }
  }
  return (g_dev != nullptr);
}

bool open_calculator_pipes() {
  if (!g_dev || !g_cfg) return false;
  const uint8_t* p = (const uint8_t*)g_cfg;
  const uint8_t* end = p + g_cfg->wTotalLength;
  p += g_cfg->bLength;
  g_ep_in = g_ep_out = 0;

  while (p + 2 <= end) {
    uint8_t len = p[0];
    uint8_t type = p[1];
    if (len == 0) break;
    if (type == USB_B_DESCRIPTOR_TYPE_ENDPOINT && len >= sizeof(usb_ep_desc_t)) {
      const usb_ep_desc_t* ep = (const usb_ep_desc_t*)p;
      if ((ep->bmAttributes & 0x03) == USB_BM_ATTRIBUTES_XFER_BULK) {
        uint8_t addr = ep->bEndpointAddress;
        if (addr & 0x80) g_ep_in = addr; else g_ep_out = addr;
        if (g_ep_in && g_ep_out) {
          Serial.printf("Endpoints IN:0x%02X OUT:0x%02X\n", g_ep_in, g_ep_out);
          return true;
        }
      }
    }
    p += len;
  }
  Serial.println("No suitable USB bulk endpoints found");
  return false;
}

bool get_string_variable(uint8_t index, String& out) {
  if (!g_dev || !g_cfg || !g_ep_in || !g_ep_out) return false;
  uint8_t req[8] = { TI_CMD_REQ, 0, 0, TI_VAR_STR, index, 0, 0, 0 };

  usb_transfer_t xfer = {};
  xfer.device_handle = g_dev;
  xfer.bEndpointAddress = g_ep_out;
  xfer.num_bytes = sizeof(req);
  xfer.data = req;
  xfer.timeout_ms = 1000;
  esp_err_t err = usb_host_transfer_submit_sync(g_usb_client, &xfer);
  if (err != ESP_OK) return false;

  uint8_t resp[256] = {0};
  xfer.bEndpointAddress = g_ep_in;
  xfer.num_bytes = sizeof(resp);
  xfer.data = resp;
  err = usb_host_transfer_submit_sync(g_usb_client, &xfer);
  if (err != ESP_OK) return false;

  if (xfer.num_bytes > 1) {
    out = String((char*)(resp + 1)); // naive parse
    return true;
  }
  return false;
}

bool set_string_variable(uint8_t index, const String& val) {
  if (!g_dev || !g_cfg || !g_ep_out) return false;
  uint8_t pkt[256] = {0};
  pkt[0] = TI_CMD_SCR; pkt[1] = TI_VAR_STR; pkt[2] = index;
  size_t n = val.length(); if (n > 252) n = 252;
  memcpy(pkt + 3, val.c_str(), n);

  usb_transfer_t xfer = {};
  xfer.device_handle = g_dev;
  xfer.bEndpointAddress = g_ep_out;
  xfer.num_bytes = 3 + n;
  xfer.data = pkt;
  xfer.timeout_ms = 1000;
  return (usb_host_transfer_submit_sync(g_usb_client, &xfer) == ESP_OK);
}

void handle_usb_events() {
  if (!g_usb_client) return;
  usb_host_client_event_msg_t msg;
  if (usb_host_client_receive(g_usb_client, &msg, 0) == ESP_OK) {
    if (msg.event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
      Serial.println("\nCalculator disconnected");
      if (g_dev && msg.free_dev.address == g_dev_addr) {
        if (g_cfg) { free((void*)g_cfg); g_cfg = nullptr; }
        usb_host_device_close(g_usb_client, g_dev);
        g_dev = nullptr; g_dev_addr = 0; g_ep_in = g_ep_out = 0;
      }
      calculatorConnected = false;
    }
  }
}

// WiFi/HTTP
static bool wifi_connect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300); Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi IP: "); Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("WiFi connection failed");
  return false;
}

static bool http_get(const String& url, String& out) {
  out = "";
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClient client; HTTPClient http;
  http.begin(client, url);
  http.setTimeout(10000);
  int code = http.GET();
  if (code == 200) { out = http.getString(); http.end(); return true; }
  Serial.printf("HTTP error: %d\n", code);
  http.end();
  return false;
}
