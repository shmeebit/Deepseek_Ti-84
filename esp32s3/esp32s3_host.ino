// ESP32-S3 USB Host bridge for TI-84 Plus CE Python - FIXED VERSION
// Goal: Calculator (USB device) <-> ESP32-S3 (USB Host + WiFi) <-> Node server (Ollama)
// Optimized for: Seeed Studio XIAO ESP32S3
// Critical: XIAO ESP32S3 requires external 5V VBUS supply via high-side switch controlled by GPIO10 (D16)

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <UrlEncode.h>

#include "../esp32/secrets.h"  // reuse server/WiFi settings

#if !CONFIG_IDF_TARGET_ESP32S3
#  error "This sketch must be built for ESP32-S3 (USB Host)"
#endif

// --- USB Host (TinyUSB / IDF) includes ---
extern "C" {
  #include "usb/usb_host.h"           // IDF USB host driver API
  #include "usb/usb_types_ch9.h"      // Descriptor types
  #include "esp_private/periph_ctrl.h"
}

// --- App settings ---
static const int PASSWORD = 69420; // keep protocol compatible with existing launcher if desired
static String serverBase = String(SERVER);

// --- Hardware: VBUS high-side switch enable pin for XIAO ESP32S3 ---
#ifndef PIN_VBUS_EN
#define PIN_VBUS_EN 10 // GPIO10 = D16 on XIAO ESP32S3
#endif

// --- TI Calculator USB Protocol Constants ---
const uint16_t TI_VID = 0x0451;   // Texas Instruments Vendor ID
const uint16_t TI_84_PID = 0xE008; // TI-84 Plus CE Python Product ID

// TI Link Protocol Commands
const uint8_t TI_CMD_READY = 0x09;
const uint8_t TI_CMD_SCR = 0x06;    // Store variable
const uint8_t TI_CMD_REQ = 0x87;    // Request variable
const uint8_t TI_VAR_STR = 0x15;    // String variable type

// --- Global State ---
bool wifiReady = false;
bool usbHostReady = false;
bool vbusEnabled = false;
bool calculatorConnected = false;

// USB handles
static usb_host_client_handle_t g_usb_client = nullptr;
static usb_host_device_handle_t g_dev = nullptr;
static uint8_t g_dev_addr = 0;
static const usb_config_desc_t* g_cfg = nullptr;
static uint8_t g_ep_in = 0, g_ep_out = 0;

// --- Function Prototypes ---
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
  Serial.println("\n[S3 Host] Deepseek_Ti-84 Bridge Starting...");

  // Initialize WiFi
  wifiReady = wifi_connect();
  Serial.printf("WiFi Status: %s\n", wifiReady ? "Connected" : "Failed");

  // Configure VBUS switch control (keep off initially)
  pinMode(PIN_VBUS_EN, OUTPUT);
  digitalWrite(PIN_VBUS_EN, LOW);
  vbusEnabled = false;

  // Initialize USB Host
  usbHostReady = init_usb_host();
  Serial.printf("USB Host Status: %s\n", usbHostReady ? "Ready" : "Failed");

  // Enable VBUS after both WiFi and USB are ready
  if (wifiReady && usbHostReady) {
    digitalWrite(PIN_VBUS_EN, HIGH);
    vbusEnabled = true;
    Serial.printf("VBUS Enabled on GPIO%d - Calculator can now connect\n", PIN_VBUS_EN);
  } else {
    Serial.println("VBUS NOT enabled - waiting for WiFi+USB initialization");
  }
}

void loop() {
  static bool pipes_open = false;
  static unsigned long last_poll = 0;
  
  // Phase 1: Wait for calculator connection
  if (usbHostReady && !calculatorConnected) {
    calculatorConnected = wait_for_calculator();
    if (calculatorConnected) {
      Serial.println("\nCalculator detected! Opening communication pipes...");
      pipes_open = open_calculator_pipes();
      if (pipes_open) {
        Serial.println("✓ Communication established! Calculator ready for commands.");
        Serial.println("  Protocol: Set Str1 with question, then Str0=\"GO\" to trigger AI query");
      } else {
        Serial.println("✗ Failed to open communication pipes");
        calculatorConnected = false;
      }
    }
  }

  // Phase 2: Handle calculator communication
  if (calculatorConnected && pipes_open && wifiReady && (millis() - last_poll > 500)) {
    last_poll = millis();
    
    // Check for trigger from calculator
    String trigger;
    if (get_string_variable(0, trigger)) {
      trigger.trim();
      
      if (trigger == "GO" || trigger == "ASK") {
        Serial.println("\n📨 Trigger detected! Reading question from Str1...");
        
        String question;
        if (get_string_variable(1, question)) {
          question.trim();
          
          if (question.length() > 0) {
            Serial.printf("❓ Question: %s\n", question.c_str());
            
            // Clear trigger to prevent re-processing
            set_string_variable(0, "WAIT");
            
            // Make HTTP request to AI server
            String url = serverBase + "/gpt/ask?question=" + urlEncode(question);
            String answer;
            
            Serial.println("🤖 Querying DeepSeek AI...");
            if (http_get(url, answer)) {
              Serial.printf("✅ Answer received (%d characters)\n", answer.length());
              
              // Truncate if too long for calculator
              if (answer.length() > 250) {
                answer = answer.substring(0, 247) + "...";
                Serial.println("⚠️ Answer truncated to fit calculator string limit");
              }
              
              // Send answer back to calculator Str2
              if (set_string_variable(2, answer)) {
                Serial.println("✅ Answer sent to Str2");
                set_string_variable(0, "DONE"); // Signal completion
              } else {
                Serial.println("❌ Failed to send answer to calculator");
                set_string_variable(0, "ERROR");
              }
            } else {
              Serial.println("❌ HTTP request failed");
              set_string_variable(2, "Network error - check server connection");
              set_string_variable(0, "ERROR");
            }
          } else {
            Serial.println("⚠️ Empty question received");
          }
        } else {
          Serial.println("❌ Failed to read question from Str1");
        }
      }
    }
    
    // Handle USB events (device disconnection)
    handle_usb_events();
  }

  delay(10);
}

// --- USB Host Implementation ---
bool init_usb_host() {
  // Install USB Host driver
  usb_host_config_t host_config = {
    .intr_flags = ESP_INTR_FLAG_LEVEL1,
  };
  
  esp_err_t err = usb_host_install(&host_config);
  if (err != ESP_OK) {
    Serial.printf("USB Host install failed: %s\n", esp_err_to_name(err));
    return false;
  }
  
  // Register USB client
  usb_host_client_config_t client_config = {
    .is_synchronous = false,
    .max_num_event_msg = 16,
    .async = {
      .client_event_callback = nullptr,
      .callback_arg = nullptr
    }
  };
  
  err = usb_host_client_register(&client_config, &g_usb_client);
  if (err != ESP_OK) {
    Serial.printf("USB Client register failed: %s\n", esp_err_to_name(err));
    usb_host_uninstall();
    return false;
  }
  
  Serial.println("✓ USB Host initialized successfully");
  return true;
}

bool wait_for_calculator() {
  if (!g_usb_client) return false;
  
  // Handle USB events
  usb_host_client_event_msg_t event_msg;
  esp_err_t err = usb_host_client_receive(g_usb_client, &event_msg, 0);
  
  if (err == ESP_OK) {
    switch (event_msg.event) {
      case USB_HOST_CLIENT_EVENT_NEW_DEV:
        g_dev_addr = event_msg.new_dev.address;
        Serial.printf("\n🔌 New USB device detected at address %d\n", g_dev_addr);
        
        // Open device
        err = usb_host_device_open(g_usb_client, g_dev_addr, &g_dev);
        if (err == ESP_OK) {
          // Check if it's a TI calculator
          usb_device_desc_t dev_desc;
          err = usb_host_get_device_desc(g_dev, &dev_desc);
          
          if (err == ESP_OK && dev_desc.idVendor == TI_VID && dev_desc.idProduct == TI_84_PID) {
            Serial.println("🎯 TI-84 Plus CE detected!");
            
            // Get configuration descriptor
            usb_host_get_active_config_desc(g_dev, &g_cfg);
            return true;
          } else {
            Serial.println("⚠️ Not a TI calculator, closing connection");
            usb_host_device_close(g_usb_client, g_dev);
            g_dev = nullptr;
          }
        }
        break;
    }
  }
  
  return (g_dev != nullptr);
}

bool open_calculator_pipes() {
  if (!g_dev || !g_cfg) return false;
  
  // Find bulk endpoints for communication
  const uint8_t* p = (const uint8_t*)g_cfg;
  const uint8_t* end = p + g_cfg->wTotalLength;
  p += g_cfg->bLength;
  
  g_ep_in = 0;
  g_ep_out = 0;
  
  while (p + 2 <= end) {
    uint8_t len = p[0];
    uint8_t type = p[1];
    
    if (len == 0) break;
    
    if (type == USB_B_DESCRIPTOR_TYPE_ENDPOINT && len >= sizeof(usb_ep_desc_t)) {
      const usb_ep_desc_t* ep = (const usb_ep_desc_t*)p;
      uint8_t attr = ep->bmAttributes & 0x03;
      
      if (attr == USB_BM_ATTRIBUTES_XFER_BULK) {
        uint8_t addr = ep->bEndpointAddress;
        if (addr & 0x80) g_ep_in = addr;
        else g_ep_out = addr;
        
        if (g_ep_in && g_ep_out) {
          Serial.printf("✓ Found USB endpoints - IN: 0x%02X, OUT: 0x%02X\n", g_ep_in, g_ep_out);
          return true;
        }
      }
    }
n    p += len;
n  }
n  \n  Serial.println("❌ No suitable USB endpoints found");
n  return false;\n}\n\nbool get_string_variable(uint8_t index, String& out) {\n  if (!g_dev || !g_cfg || !g_ep_in || !g_ep_out) return false;\n  \n  // Send request for string variable (simplified TI protocol)\n  uint8_t request_packet[8] = {\n    TI_CMD_REQ,  // Request variable command\n    0x00, 0x00,  // Reserved\n    TI_VAR_STR,  // Variable type (string)\n    index,       // Variable index (0-9)\n    0x00, 0x00,  // Reserved\n    0x00         // Checksum (simplified for now)\n  };\n  \n  // Send the request\n  usb_transfer_t transfer = {};\n  transfer.device_handle = g_dev;\n  transfer.bEndpointAddress = g_ep_out;\n  transfer.num_bytes = sizeof(request_packet);\n  transfer.data = request_packet;\n  transfer.timeout_ms = 1000;\n  \n  esp_err_t err = usb_host_transfer_submit_sync(g_usb_client, &transfer);\n  if (err != ESP_OK) {\n    Serial.printf("❌ Failed to send request: %s\n", esp_err_to_name(err));\n    return false;\n  }\n  \n  // Receive response\n  uint8_t response_buffer[256];\n  transfer.bEndpointAddress = g_ep_in;\n  transfer.num_bytes = sizeof(response_buffer);\n  transfer.data = response_buffer;\n  \n  err = usb_host_transfer_submit_sync(g_usb_client, &transfer);\n  if (err != ESP_OK) {\n    Serial.printf("❌ Failed to receive response: %s\n", esp_err_to_name(err));\n    return false;\n  }\n  \n  // Parse response (simplified - assumes valid TI protocol response)\n  if (transfer.num_bytes > 0) {\n    // Skip protocol header and extract string data\n    size_t data_start = 1; // Simplified - real implementation would parse TI protocol properly\n    if (data_start < transfer.num_bytes) {\n      out = String((char*)(response_buffer + data_start));\n      return true;\n    }\n  }\n  \n  return false;\n}\n\nbool set_string_variable(uint8_t index, const String& val) {\n  if (!g_dev || !g_cfg || !g_ep_out) return false;\n  \n  // Prepare data packet (simplified TI protocol)\n  uint8_t data_packet[256];\n  data_packet[0] = TI_CMD_SCR;  // Store variable command\n  data_packet[1] = TI_VAR_STR;  // String variable type\n  data_packet[2] = index;       // Variable index\n  \n  // Copy string data\n  size_t str_len = val.length();\n  if (str_len > 252) str_len = 252; // Limit packet size for safety\n  \n  memcpy(data_packet + 3, val.c_str(), str_len);\n  size_t packet_size = 3 + str_len;\n  \n  // Send data to calculator\n  usb_transfer_t transfer = {};\n  transfer.device_handle = g_dev;\n  transfer.bEndpointAddress = g_ep_out;\n  transfer.num_bytes = packet_size;\n  transfer.data = data_packet;\n  transfer.timeout_ms = 1000;\n  \n  esp_err_t err = usb_host_transfer_submit_sync(g_usb_client, &transfer);\n  if (err != ESP_OK) {\n    Serial.printf("❌ Failed to send data: %s\n", esp_err_to_name(err));\n    return false;\n  }\n  \n  return true;\n}\n\nvoid handle_usb_events() {\n  if (!g_usb_client) return;\n  \n  usb_host_client_event_msg_t msg;\n  esp_err_t err = usb_host_client_receive(g_usb_client, &msg, 0);\n  \n  if (err == ESP_OK) {\n    if (msg.event == USB_HOST_CLIENT_EVENT_DEV_GONE) {\n      Serial.println("\n🔌 Calculator disconnected!\");\n      calculatorConnected = false;\n      \n      if (g_dev && msg.free_dev.address == g_dev_addr) {\n        if (g_cfg) {\n          free((void*)g_cfg);\n          g_cfg = nullptr;\n        }\n        usb_host_device_close(g_usb_client, g_dev);\n        g_dev = nullptr;\n        g_dev_addr = 0;\n        g_ep_in = 0;\n        g_ep_out = 0;\n      }\n    }\n  }\n}\n\n// --- WiFi and HTTP Helper Functions ---\nstatic bool wifi_connect() {\n  WiFi.mode(WIFI_STA);\n  WiFi.begin(WIFI_SSID, WIFI_PASS);\n  \n  unsigned long start = millis();\n  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {\n    delay(300);\n    Serial.print('.');\n  }\n  Serial.println();\n  \n  if (WiFi.status() == WL_CONNECTED) {\n    Serial.print("✓ WiFi Connected - IP: ");\n    Serial.println(WiFi.localIP());\n    return true;\n  }\n  \n  Serial.println("❌ WiFi Connection Failed\");\n  return false;\n}\n\nstatic bool http_get(const String& url, String& out) {\n  out = \"\";\n  if (!wifiReady) return false;\n  \n  WiFiClient client;\n  HTTPClient http;\n  \n  http.begin(client, url);\n  http.setTimeout(10000); // 10 second timeout\n  \n  int code = http.GET();\n  if (code == 200) {\n    out = http.getString();\n    http.end();\n    return true;\n  }\n  \n  Serial.printf(\"❌ HTTP Error: %d\n\", code);\n  http.end();\n  return false;\n}