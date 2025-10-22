// ESP32-S3 USB Host bridge for TI-84 Plus CE Python
// Goal: Calculator (USB device) <-> ESP32-S3 (USB Host + WiFi) <-> Node server (Ollama)
// Status: Scaffold - enumerates device (to be implemented) and preserves existing HTTP bridge structure.
// Note: Requires an ESP32-S3 board that supports USB OTG Host and can SOURCE 5V VBUS to the USB port.
//       XIAO ESP32S3 Plus may not expose host VBUS. Verify board docs; otherwise use ESP32-S3-USB-OTG devkit.

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <UrlEncode.h>

#include "../esp32/secrets.h"  // reuse server/WiFi settings

#if !CONFIG_IDF_TARGET_ESP32S3
#  error "This sketch must be built for ESP32-S3 (USB Host)"
#endif

// --- USB Host (TinyUSB / IDF) includes ---
// Arduino-ESP32 exposes IDF headers for host in recent cores, otherwise use IDF directly
extern "C" {
  #include "usb/usb_host.h"           // IDF USB host driver API
  #include "usb/usb_types_ch9.h"      // Descriptor types
  #include "esp_private/periph_ctrl.h"
}

// --- App settings ---
static const int PASSWORD = 69420; // keep protocol compatible with existing launcher if desired
static String serverBase = String(SERVER);

// --- Hardware: VBUS high-side switch enable pin ---
// Default: GPIO10 which is D16 on XIAO ESP32S3 Plus (per Seeed pinout)
// This pin drives the EN of your 5V high-side switch that sources USB VBUS to the calculator.
// Change this if you wire EN to a different pin.
#ifndef PIN_VBUS_EN
#define PIN_VBUS_EN 10 // GPIO10 (D16)
#endif

// --- State ---
bool wifiReady = false;
bool usbHostReady = false;
bool vbusEnabled = false;

// USB device state
static usb_host_device_handle_t g_dev = nullptr;
static uint8_t g_dev_addr = 0;
static const usb_config_desc_t* g_cfg = nullptr;
static uint8_t g_ep_in = 0, g_ep_out = 0; // discovered bulk endpoints

static bool find_bulk_endpoints(const usb_config_desc_t* cfg, uint8_t& ep_in, uint8_t& ep_out) {
  if (!cfg) return false;
  const uint8_t* p = reinterpret_cast<const uint8_t*>(cfg);
  const uint8_t* end = p + cfg->wTotalLength;
  p += cfg->bLength; // skip config header
  ep_in = ep_out = 0;
  while (p + 2 <= end) {
    uint8_t len = p[0];
    uint8_t type = p[1];
    if (len == 0) break;
    if (type == USB_B_DESCRIPTOR_TYPE_ENDPOINT && len >= sizeof(usb_ep_desc_t)) {
      const usb_ep_desc_t* ep = reinterpret_cast<const usb_ep_desc_t*>(p);
      uint8_t attr = ep->bmAttributes & 0x03; // transfer type
      if (attr == USB_BM_ATTRIBUTES_XFER_BULK) {
        uint8_t addr = ep->bEndpointAddress;
        if (addr & 0x80) ep_in = addr; else ep_out = addr;
        if (ep_in && ep_out) return true;
      }
    }
    p += len;
  }
  return (ep_in || ep_out);
}

// Placeholder: TI USB endpoints/handles
static usb_host_client_handle_t g_usb_client = nullptr; // client to receive events

// Forward decls
static bool wifi_connect();
static bool http_get(const String& url, String& out);

// TI-USB link layer (placeholders)
namespace tiusb {
  bool init_host();                   // bring up USB host stack
  bool wait_for_calculator();         // block until TI-84 CE Python enumerates
  bool open_pipes();                  // claim interfaces/endpoints
  bool get_str(uint8_t index, String& out); // read StrN (silent transfer)
  bool set_str(uint8_t index, const String& val);
  bool get_real(char name, double& out);
  bool set_real(char name, double val);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("[S3 Host] Boot");

  // WiFi
  wifiReady = wifi_connect();
  Serial.printf("WiFi: %s\n", wifiReady ? "connected" : "not connected");

  // Configure VBUS switch control (keep off for now)
  pinMode(PIN_VBUS_EN, OUTPUT);
  digitalWrite(PIN_VBUS_EN, LOW);
  vbusEnabled = false;

  // Enable USB Host driver (IDF API)
  usbHostReady = tiusb::init_host();
  Serial.printf("USB Host: %s\n", usbHostReady ? "ready" : "failed");

  // After WiFi and USB host init, enable 5V VBUS to power the calculator
  if (wifiReady && usbHostReady) {
    digitalWrite(PIN_VBUS_EN, HIGH);
    vbusEnabled = true;
    Serial.printf("VBUS: enabled on GPIO%d\n", PIN_VBUS_EN);
  } else {
    Serial.println("VBUS: NOT enabled (waiting for WiFi+USB host)");
  }
}

void loop() {
  // Minimal demo: enumerate + poll events
  static bool attached = false;
  if (usbHostReady && !attached) {
    attached = tiusb::wait_for_calculator();
    if (attached) {
      Serial.println("Calculator attached. Opening pipes...");
      if (tiusb::open_pipes()) {
        Serial.println("Pipes open.");
      } else {
        Serial.println("Failed to open pipes.");
      }
    }
  }

  // TODO: Once endpoints are known, implement a small command loop similar to the CBL2 path:
  // - Pull a command string/real from calc (silent transfer)
  // - Make HTTP call to server
  // - Push response back to calc (paged)

  delay(50);
}

// ---- WiFi helpers ----
static bool wifi_connect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    return true;
  }
  return false;
}

static bool http_get(const String& url, String& out) {
  out = "";
  if (!wifiReady) return false;
  WiFiClient client;
  HTTPClient http;
  http.begin(client, url);
  int code = http.GET();
  if (code != 200) { http.end(); return false; }
  out = http.getString();
  http.end();
  return true;
}

// ---- TI USB (placeholders) ----
namespace tiusb {
  static bool host_initialized = false;

  bool init_host() {
    // Install USB Host driver
    usb_host_config_t host_config = {
      .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    esp_err_t err = usb_host_install(&host_config);
    if (err == ESP_OK) {
      host_initialized = true;
      Serial.println("USB Host installed.");
      // Create a synchronous client to receive events from the host lib
      usb_host_client_config_t client_config = {
        .is_synchronous = true,
        .max_num_event_msg = 16,
      };
      if (usb_host_client_register(&client_config, &g_usb_client) == ESP_OK) {
        Serial.println("USB client registered.");
        return true;
      } else {
        Serial.println("usb_host_client_register failed");
      }
    } else {
      Serial.printf("usb_host_install failed: %d\n", (int)err);
    }
    return false;
  }

  bool wait_for_calculator() {
    if (!host_initialized || !g_usb_client) return false;
    // Poll for events and react to new device
    usb_host_client_event_msg_t msg;
    esp_err_t err = usb_host_client_receive(g_usb_client, &msg, 50);
    if (err == ESP_OK) {
      if (msg.event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        g_dev_addr = msg.new_dev.address;
        Serial.printf("USB: NEW_DEV addr=%u\n", g_dev_addr);
        if (usb_host_device_open(g_usb_client, g_dev_addr, &g_dev) == ESP_OK) {
          // Read device descriptor
          usb_device_desc_t dev_desc = {};
          if (usb_host_get_device_desc(g_dev, &dev_desc) == ESP_OK) {
            Serial.printf("USB: VID=0x%04X PID=0x%04X, bcdUSB=0x%04X, class=0x%02X\n",
              dev_desc.idVendor, dev_desc.idProduct, dev_desc.bcdUSB, dev_desc.bDeviceClass);
          }
          // Read active configuration descriptor
          const usb_config_desc_t* cfg = nullptr;
          if (usb_host_get_active_config_descriptor(g_dev, &cfg) == ESP_OK && cfg) {
            g_cfg = cfg;
            Serial.printf("USB: Config wTotalLength=%u, bNumInterfaces=%u\n",
              cfg->wTotalLength, cfg->bNumInterfaces);
          }
          return true;
        } else {
          Serial.println("usb_host_device_open failed");
        }
      } else if (msg.event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        // A device disappeared that we didn't open
        Serial.println("USB: DEV_GONE");
      } else if (msg.event == USB_HOST_CLIENT_EVENT_FREE_DEV) {
        // Close and free device if it matches our address
        if (g_dev && msg.free_dev.address == g_dev_addr) {
          Serial.println("USB: FREE_DEV (closing)");
          if (g_cfg) { free((void*)g_cfg); g_cfg = nullptr; }
          usb_host_device_close(g_usb_client, g_dev);
          g_dev = nullptr;
          g_dev_addr = 0;
          // Optionally disable VBUS to force a clean reprobe
          if (vbusEnabled) { digitalWrite(PIN_VBUS_EN, LOW); vbusEnabled = false; Serial.println("VBUS: disabled"); }
        }
      }
    }
    return (g_dev != nullptr);
  }

  bool open_pipes() {
    // TODO: parse configuration/interface descriptors, find bulk endpoints
    // that TI link protocol uses, and prepare transfers.
    if (!g_dev || !g_cfg) {
      Serial.println("open_pipes: no device/config yet");
      return false;
    }
    Serial.println("open_pipes: parsing endpoints");
    if (find_bulk_endpoints(g_cfg, g_ep_in, g_ep_out)) {
      Serial.printf("Found bulk endpoints: IN=0x%02X OUT=0x%02X\n", g_ep_in, g_ep_out);
      // TODO: claim interface and prepare transfer objects
      return true; // endpoints identified
    } else {
      Serial.println("No bulk endpoints found yet");
      return false;
    }
  }

  bool get_str(uint8_t, String&) { return false; }
  bool set_str(uint8_t, const String&) { return false; }
  bool get_real(char, double&) { return false; }
  bool set_real(char, double) { return false; }
}
