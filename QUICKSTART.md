# TI-32 Quick Start Guide

Get your calculator talking to AI in 15 minutes!

## What You Need

### Hardware
- TI-84 Plus CE (Python Edition)
- **Seeed Studio XIAO ESP32S3** (recommended - compact & battery-ready!)
- 5V power circuit (boost converter + switch) - *XIAO requires external VBUS*
- USB cable for calculator connection
- Computer running Windows/Mac/Linux

**📖 See [XIAO_ESP32S3_GUIDE.md](XIAO_ESP32S3_GUIDE.md) for XIAO-specific setup!**

### Software
- Node.js (https://nodejs.org/)
- Ollama (https://ollama.ai/)
- Arduino IDE with ESP32 support
- TI Connect CE (for transferring Python programs)

## 5-Minute Setup

### 1. Server (3 minutes)

```bash
# Install and start Ollama
ollama pull deepseek-r1:latest
ollama serve

# In a new terminal:
cd server
copy .env.example .env
npm install
npm start
```

Server should show: `TI-32 Server with DeepSeek-R1 listening on port 8080`

### 2. ESP32 Firmware (5 minutes)

1. Get your computer's IP address:
   - Windows: `ipconfig`
   - Mac/Linux: `ifconfig` or `ip addr`

2. Edit `esp32/secrets.h`:
   ```cpp
   #define WIFI_SSID "YourWiFiName"
   #define WIFI_PASS "YourWiFiPassword"
   #define SERVER "http://192.168.1.XXX:8080"  // Your IP here
   ```

3. Flash to XIAO ESP32S3:
   - Open `esp32s3/esp32s3_host.ino` in Arduino IDE
   - Select board: **XIAO_ESP32S3** (Tools → Board → esp32)
   - Set **USB CDC On Boot: Enabled**
   - Click Upload
   - Open Serial Monitor (115200) - should see "WiFi: connected"
   - **Important**: Wire 5V VBUS circuit (see XIAO_ESP32S3_GUIDE.md)

### 3. Calculator Program (2 minutes)

1. Connect TI-84 to computer
2. Open TI Connect CE
3. Send `ti84/simple_test.py` to calculator
4. Disconnect from computer
5. Connect calculator to ESP32 via USB

### 4. Test! (30 seconds)

1. On calculator, run `simple_test`
2. Watch it ask "What is 2+2?"
3. See the AI respond!
4. Check ESP32 Serial Monitor for detailed logs

## What's Happening?

```
[Calculator] → Str1="What is 2+2?", Str0="GO"
     ↓ USB
[ESP32-S3] → Reads strings, makes HTTP request
     ↓ WiFi
[Your PC Server] → Forwards to Ollama
     ↓ 
[Ollama/DeepSeek] → Generates answer
     ↓
[Your PC Server] → Returns to ESP32
     ↓ WiFi
[ESP32-S3] → Writes Str2="4", Str0="DONE"
     ↓ USB
[Calculator] → Displays answer!
```

## Next Steps

1. **Try the full program**: Transfer `ti84/ti32_ai.py` for interactive menu
2. **Ask real questions**: "Explain the Pythagorean theorem"
3. **Get math help**: "Solve x^2 + 5x + 6 = 0"
4. **Add battery power**: Follow `pcb/README.md` for portable design

## Common Issues

### "Timeout waiting for response"
- Check ESP32 Serial Monitor - is WiFi connected?
- Ping your server: `curl http://YOUR_IP:8080/health`
- Is Ollama running? Check with `ollama list`

### "USB Host: failed"
- Make sure you have an ESP32-**S3** (not regular ESP32)
- Check board selection in Arduino IDE

### Server not responding
```bash
# Check if Ollama is running
ollama serve

# Check if server is running
cd server
npm start

# Test server
curl http://localhost:8080/health
```

### Calculator not detected
- ESP32 Serial Monitor should show "NEW_DEV" when calculator connects
- Check USB cable
- Try unplugging and replugging calculator

## Tips

- Keep Serial Monitor open when debugging
- Start with `simple_test.py` before `ti32_ai.py`
- Questions should be under 255 characters
- Answers are truncated to 255 chars to fit calculator limits

## Need Help?

1. Check Serial Monitor output on ESP32
2. Test server endpoint: `curl "http://localhost:8080/gpt/ask?question=test"`
3. Review full README.md for detailed troubleshooting
4. Check that all three components are running (Ollama, Server, ESP32)

## Success Checklist

- [ ] Ollama running and model pulled
- [ ] Server started and health check passes
- [ ] ESP32 shows "WiFi: connected" in Serial Monitor
- [ ] ESP32 shows "USB Host: ready"
- [ ] Calculator program transferred
- [ ] Calculator connected to ESP32 via USB
- [ ] Simple test completes successfully

**Ready to go!** 🚀

---

*For battery-powered portable use, see the full PCB guide in `pcb/README.md`*
