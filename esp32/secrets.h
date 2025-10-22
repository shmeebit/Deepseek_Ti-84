#ifndef SECRETS_H
#define SECRETS_H

// HTTP Authentication (optional - leave empty if not used)
#define HTTP_USERNAME "" 
#define HTTP_PASSWORD "" 

// WiFi Configuration
#define WIFI_SSID "YourWiFiSSID"     // Your WiFi network name
#define WIFI_PASS "YourWiFiPassword" // Your WiFi password

// Server Configuration
// Replace with your computer's IP address running the Ollama server
// Example: "http://192.168.1.100:8080"
#define SERVER "http://192.168.1.XXX:8080"

// Chat Configuration
#define CHAT_NAME "YourName" // Your name for the chat feature (3 chars max recommended)

#endif // SECRETS_H
