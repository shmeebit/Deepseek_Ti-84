// ESP32-S3 USB Host bridge for TI-84 Plus CE Python
// Goal: Calculator (USB device) <-> ESP32-S3 (USB Host + WiFi) <-> Node server (Ollama)
// Optimized for: Seeed Studio XIAO ESP32S3
// Note: XIAO ESP32S3 requires external 5V VBUS supply via high-side switch controlled by GPIO10 (D16).
//       The XIAO does not provide 5V on its USB port by default - you must add external power circuit.

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

// --- Hardware: VBUS high-side switch enable pin for XIAO ESP32S3 ---
// GPIO10 (labeled D16 on XIAO ESP32S3 pinout)
// This pin drives the EN of your 5V high-side switch that sources USB VBUS to the calculator.
// On XIAO ESP32S3, the USB-C port does NOT provide 5V output - you must add external circuit.
// Recommended: 5V boost converter → high-side switch (AP2331/TPS2553) → calculator VBUS
#ifndef PIN_VBUS_EN
#define PIN_VBUS_EN 10 // GPIO10 = D16 on XIAO ESP32S3
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
  // Phase 1: Wait for calculator attachment
  static bool attached = false;
  static bool pipes_open = false;
  static unsigned long last_poll = 0;
  
  if (usbHostReady && !attached) {
    attached = tiusb::wait_for_calculator();
    if (attached) {
      Serial.println("Calculator attached. Opening pipes...");
      if (tiusb::open_pipes()) {
        Serial.println("Pipes open. Ready for commands!");
        Serial.println("Calculator should set Str1 with question, then set Str0=\"GO\" to trigger");
        pipes_open = true;
      } else {
        Serial.println("Failed to open pipes.");
        attached = false; // retry
      }
    }
  }

  // Phase 2: Poll for commands from calculator
  // Protocol: Calculator writes question to Str1, then writes "GO" to Str0 to signal ready
  if (attached && pipes_open && wifiReady && millis() - last_poll > 500) {
    last_poll = millis();
    
    String trigger;
    if (tiusb::get_str(0, trigger)) {
      trigger.trim();
      if (trigger == "GO" || trigger == "ASK") {
        Serial.println("Trigger detected! Reading question from Str1...");
        
        String question;
        if (tiusb::get_str(1, question)) {
          question.trim();
          
          if (question.length() > 0) {
            Serial.printf("Question: %s\n", question.c_str());
            
            // Clear trigger
            tiusb::set_str(0, "WAIT");
            
            // Make HTTP request to server
            String url = serverBase + "/gpt/ask?question=" + urlEncode(question);
            String answer;
            
            Serial.println("Querying DeepSeek...");
            if (http_get(url, answer)) {
              Serial.printf("Answer received (%d chars)\n", answer.length());
              
              // If answer is too long, truncate or page it
              if (answer.length() > 255) {
                answer = answer.substring(0, 252) + "...";
                Serial.println("Answer truncated to fit calculator string limit");
              }
              
              // Send answer back to Str2
              if (tiusb::set_str(2, answer)) {
                Serial.println("Answer sent to Str2");
                // Signal completion
                tiusb::set_str(0, "DONE");
              } else {
                Serial.println("Failed to send answer");
                tiusb::set_str(0, "ERROR");
              }
            } else {
              Serial.println("HTTP request failed");
              tiusb::set_str(2, "Network error");
              tiusb::set_str(0, "ERROR");
            }
          }
        } else {
          Serial.println("Failed to read Str1");
        }
      }
    }
    
    // Handle USB events to detect disconnection
    usb_host_client_event_msg_t msg;
    if (usb_host_client_receive(g_usb_client, &msg, 0) == ESP_OK) {
      if (msg.event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        Serial.println("Calculator disconnected");
        attached = false;
        pipes_open = false;
        if (g_dev && msg.free_dev.address == g_dev_addr) {
          if (g_cfg) { free((void*)g_cfg); g_cfg = nullptr; }
          usb_host_device_close(g_usb_client, g_dev);
          g_dev = nullptr;
          g_dev_addr = 0;
        }
      }
    }
  }

  delay(10);
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
    if (!g_dev || !g_cfg) {
      Serial.println("open_pipes: no device/config yet");
      return false;
    }
    Serial.println("open_pipes: parsing endpoints");
    if (find_bulk_endpoints(g_cfg, g_ep_in, g_ep_out)) {
      Serial.printf("Found bulk endpoints: IN=0x%02X OUT=0x%02X\n", g_ep_in, g_ep_out);
      
      // Claim interface 0 (TI calculators typically use interface 0 for USB communication)
      esp_err_t err = usb_host_interface_claim(g_usb_client, g_dev, 0, 0);
      if (err != ESP_OK) {
        Serial.printf("Failed to claim interface 0: %d\n", (int)err);
        return false;
      }
      Serial.println("Interface 0 claimed successfully");
      
      return true; // endpoints identified and interface claimed
    } else {
      Serial.println("No bulk endpoints found yet");
      return false;
    }
  }

  // TI Link Protocol Commands (simplified for string variables)
  // Based on TI-84 Plus CE link protocol specification
  static const uint8_t CMD_VAR_REQUEST = 0x09;    // Request variable
  static const uint8_t CMD_VAR_CONTENTS = 0x06;   // Variable contents
  static const uint8_t CMD_ACK = 0x56;            // Acknowledgement
  static const uint8_t CMD_CONTINUE = 0x09;       // Continue/Ready
  static const uint8_t TYPE_STR = 0x04;           // String variable type
  
  // Helper: send packet to calculator
  bool send_packet(const uint8_t* data, size_t len, uint32_t timeout_ms = 1000) {
    if (!g_dev || !g_ep_out) return false;
    
    usb_transfer_t* transfer;
    esp_err_t err = usb_host_transfer_alloc(len, 0, &transfer);
    if (err != ESP_OK) return false;
    
    memcpy(transfer->data_buffer, data, len);
    transfer->device_handle = g_dev;
    transfer->bEndpointAddress = g_ep_out;
    transfer->num_bytes = len;
    transfer->timeout_ms = timeout_ms;
    
    err = usb_host_transfer_submit(transfer);
    if (err == ESP_OK) {
      // Wait for completion (synchronous)
      unsigned long start = millis();
      while (transfer->status == USB_TRANSFER_STATUS_PENDING && millis() - start < timeout_ms) {
        delay(1);
      }
    }
    
    bool success = (transfer->status == USB_TRANSFER_STATUS_COMPLETED);
    usb_host_transfer_free(transfer);
    return success;
  }
  
  // Helper: receive packet from calculator
  bool recv_packet(uint8_t* data, size_t max_len, size_t& actual_len, uint32_t timeout_ms = 1000) {
    if (!g_dev || !g_ep_in) return false;
    
    usb_transfer_t* transfer;
    esp_err_t err = usb_host_transfer_alloc(max_len, 0, &transfer);
    if (err != ESP_OK) return false;
    
    transfer->device_handle = g_dev;
    transfer->bEndpointAddress = g_ep_in;
    transfer->num_bytes = max_len;
    transfer->timeout_ms = timeout_ms;
    
    err = usb_host_transfer_submit(transfer);
    if (err == ESP_OK) {
      unsigned long start = millis();
      while (transfer->status == USB_TRANSFER_STATUS_PENDING && millis() - start < timeout_ms) {
        delay(1);
      }
    }
    
    bool success = false;
    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED) {
      actual_len = transfer->actual_num_bytes;
      memcpy(data, transfer->data_buffer, actual_len);
      success = true;
    }
    
    usb_host_transfer_free(transfer);
    return success;
  }

  // Read string variable from calculator (simplified implementation)
  // varIndex: 0-9 for Str0-Str9 
  bool get_str(uint8_t varIndex, String& out) {
    if (!g_dev || varIndex > 9) return false;
    
    // Build variable name (e.g., "Str1")
    char varName[5];
    snprintf(varName, sizeof(varName), "Str%d", varIndex);
    
    // TI Link protocol: Request variable packet
    // Format: [0x01, cmd, len_lo, len_hi, type, namelen, name..., checksum_lo, checksum_hi]
    uint8_t req[16];
    req[0] = 0x01; // Machine ID (computer)
    req[1] = CMD_VAR_REQUEST;
    req[2] = 5; // data length low
    req[3] = 0; // data length high
    req[4] = TYPE_STR;
    req[5] = 4; // name length (e.g., "Str1")
    memcpy(&req[6], varName, 4);
    
    // Calculate checksum (sum of data bytes)
    uint16_t checksum = 0;
    for (int i = 4; i < 10; i++) checksum += req[i];
    req[10] = checksum & 0xFF;
    req[11] = (checksum >> 8) & 0xFF;
    
    Serial.printf("Requesting %s from calculator...\n", varName);
    
    if (!send_packet(req, 12, 2000)) {
      Serial.println("Failed to send variable request");
      return false;
    }
    
    // Wait for acknowledgement or variable contents
    uint8_t resp[256];
    size_t resp_len = 0;
    
    if (!recv_packet(resp, sizeof(resp), resp_len, 2000)) {
      Serial.println("No response from calculator");
      return false;
    }
    
    // Parse response - look for CMD_VAR_CONTENTS (0x06) or error
    if (resp_len < 4 || resp[1] != CMD_VAR_CONTENTS) {
      Serial.printf("Unexpected response: cmd=0x%02X\n", resp_len > 1 ? resp[1] : 0);
      return false;
    }
    
    // Extract string data from packet
    // Format: [0x01, 0x06, len_lo, len_hi, data_len_lo, data_len_hi, ...string..., checksum]
    uint16_t data_len = resp[4] | (resp[5] << 8);
    if (data_len > 0 && resp_len >= 6 + data_len) {
      out = "";
      for (uint16_t i = 0; i < data_len; i++) {
        out += (char)resp[6 + i];
      }
      Serial.printf("Received: '%s'\n", out.c_str());
      return true;
    }
    
    return false;
  }

  // Write string variable to calculator
  bool set_str(uint8_t varIndex, const String& val) {
    if (!g_dev || varIndex > 9) return false;
    
    char varName[5];
    snprintf(varName, sizeof(varName), "Str%d", varIndex);
    
    // TI Link protocol: Variable contents packet
    uint8_t packet[256];
    uint16_t str_len = val.length();
    
    packet[0] = 0x01; // Machine ID
    packet[1] = CMD_VAR_CONTENTS;
    
    // Calculate total data length: type(1) + namelen(1) + name(4) + datalen(2) + data
    uint16_t data_len = 1 + 1 + 4 + 2 + str_len;
    packet[2] = data_len & 0xFF;
    packet[3] = (data_len >> 8) & 0xFF;
    
    packet[4] = TYPE_STR;
    packet[5] = 4; // name length
    memcpy(&packet[6], varName, 4);
    packet[10] = str_len & 0xFF;
    packet[11] = (str_len >> 8) & 0xFF;
    
    // Copy string data
    for (uint16_t i = 0; i < str_len; i++) {
      packet[12 + i] = val[i];
    }
    
    // Calculate checksum
    uint16_t checksum = 0;
    for (uint16_t i = 4; i < 12 + str_len; i++) {
      checksum += packet[i];
    }
    packet[12 + str_len] = checksum & 0xFF;
    packet[13 + str_len] = (checksum >> 8) & 0xFF;
    
    Serial.printf("Sending '%s' to %s...\n", val.c_str(), varName);
    
    if (!send_packet(packet, 14 + str_len, 2000)) {
      Serial.println("Failed to send variable contents");
      return false;
    }
    
    // Wait for ACK
    uint8_t resp[16];
    size_t resp_len = 0;
    if (recv_packet(resp, sizeof(resp), resp_len, 2000)) {
      if (resp_len >= 2 && resp[1] == CMD_ACK) {
        Serial.println("Calculator acknowledged");
        return true;
      }
    }
    
    return false;
  }

  bool get_real(char, double&) { return false; }
  bool set_real(char, double) { return false; }
}
