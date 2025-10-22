# TI‑32 (ESP32‑S3 USB Host + Ollama DeepSeek‑R1)

Portable calculator ↔ ESP32‑S3 bridge to a local LLM via Ollama.

- TI‑84 Plus CE Python connects over USB to an ESP32‑S3 acting as a USB host.
- The ESP connects over Wi‑Fi to a Node.js server that proxies to your local Ollama model (DeepSeek‑R1 by default).

## Repo layout

- **esp32s3/** – ESP32‑S3 host firmware (USB host + Wi‑Fi bridge with TI link protocol)
- **esp32/** – Shared config: `secrets.h` (Wi‑Fi/server credentials)
- **server/** – Minimal Node/Express bridge to Ollama (`/gpt/ask` and `/gpt/solve` endpoints)
- **ti84/** – Python programs for TI-84 Plus CE calculator
- **pcb/** – Carrier PCB guide (schematic blocks, BOM, layout for battery-powered design)

## Quick start

**Recommended Hardware**: Seeed Studio XIAO ESP32S3 (compact, battery-ready, perfect for this project!)

1) **Server + Ollama**
   - Install Ollama and pull a DeepSeek‑R1 variant (e.g., `deepseek-r1:latest`).
   - In `server/`, copy `.env.example` to `.env` and set `OLLAMA_URL` and `OLLAMA_MODEL`.
   - Start the server. It exposes `/gpt` and helper endpoints.
   
2) **ESP32‑S3 firmware (XIAO ESP32S3)**
   - See **[XIAO_ESP32S3_GUIDE.md](XIAO_ESP32S3_GUIDE.md)** for complete setup instructions
   - Edit `esp32/secrets.h` with your Wi‑Fi and server base URL.
   - Flash `esp32s3/esp32s3_host.ino` using Arduino IDE
   - **Important**: XIAO requires external 5V VBUS circuit (boost converter + high‑side switch on GPIO10/D16)
   - Open Serial Monitor (115200) to confirm Wi‑Fi connects and USB Host installs.
   
3) **Connect the calculator**
   - Transfer Python program from `ti84/` to your TI-84 Plus CE
   - Connect calculator to XIAO ESP32S3 via USB
   - Ensure 5V VBUS is provided to calculator (required for USB enumeration)

## Current Status

✅ **Server** - Fully functional Node/Express bridge to Ollama with DeepSeek-R1
✅ **ESP32‑S3 firmware** - Complete implementation with:
  - WiFi connectivity
  - USB host driver
  - TI link protocol (string variable transfers)
  - Command loop for processing calculator queries
✅ **TI-84 Programs** - Python programs ready to transfer to calculator
✅ **PCB Design** - Complete guide and starter KiCad project

**Ready to build and test!**

Notes
- Use a USB‑C OTG/host adapter to ensure the ESP32‑S3 takes host role; it does not supply 5V by itself.
- Assert VBUS (enable your switch) after Wi‑Fi is up, then the USB host driver will enumerate the calculator when plugged.

## How It Works

1. **Calculator Side**: TI-84 Python program writes question to `Str1`, sets `Str0="GO"`
2. **ESP32 Side**: Polls for `Str0="GO"`, reads question from `Str1` via TI link protocol over USB
3. **Server Side**: ESP32 makes HTTP GET to `/gpt/ask?question=...`, Ollama/DeepSeek processes
4. **Response**: ESP32 writes answer to `Str2`, sets `Str0="DONE"`
5. **Display**: Calculator reads `Str2` and displays answer to user

## Future Enhancements

Optional improvements you can add:

- **Streaming responses** - Real-time token streaming for longer answers
- **Authentication** - Add auth header check in server for LAN security
- **Real variables** - Implement `get_real()` / `set_real()` for numeric calculations
- **File transfer** - Extend protocol to support Python file transfer
- **Multiple models** - Switch between different Ollama models
- **History** - Store Q&A history on calculator
- **Battery monitoring** - Add low-battery detection and VBUS control

## Battery power plan

Yes, you can run the ESP from a battery. For USB host mode you still must provide a stable 5V VBUS to the calculator. A typical path:

1) Battery (e.g., 3.7V LiPo, 1000–2000 mAh)
2) LiPo charge/management (onboard on some XIAO variants; otherwise use a charger board)
3) 5V boost converter (≥ 1 A recommended)
4) High‑side USB power switch with current limit (e.g., AP2331, TPS2553) controlled by an ESP32‑S3 GPIO
5) VBUS of the device port that the calculator plugs into

Wiring notes
- The ESP32‑S3 runs at 3.3V internally. Battery powers the board via its battery input; the 5V boost is only for USB VBUS to the calculator.
- Common ground: connect the boost/switch ground to the ESP32 ground.
- Port choice: Using a USB‑A receptacle for the calculator is simplest. If you use USB‑C, add 5.1 kΩ pull‑ups (Rp) on CC1/CC2 to advertise 5V source.
- Current budget: provision 500 mA for the calculator (typical draw is lower, but headroom avoids brown‑outs).
- Safety: pick a switch with over‑current and thermal shutdown; monitor its FAULT pin if available.
- Avoid back‑powering: don’t feed 5V back into your PC’s USB when programming the ESP32. Use power‑path diodes or only enable VBUS when on battery.

Default EN pin
- We default to GPIO10 (labeled D16 on the XIAO ESP32S3 Plus) to drive the high‑side switch EN. Change `PIN_VBUS_EN` in `esp32s3/esp32s3_host.ino` if you wire a different pin.

Firmware timing
- Bring up Wi‑Fi → initialize USB host → enable VBUS (GPIO) → wait for attach → open endpoints.
- Disable VBUS on detach or low battery conditions.

## Wiring guide (step‑by‑step)

Parts
- XIAO ESP32S3 Plus
- Single‑cell LiPo battery (3.7V)
- LiPo charge/management (onboard on some XIAO variants or an external charger board)
- 5V boost converter (≥1 A)
- High‑side USB power switch with current limit (AP2331, TPS2553, or similar)
- USB‑A receptacle for the calculator cable (or USB‑C with 5.1 kΩ CC pull‑ups)
- Wires, optionally a small perfboard

Connections
1) Battery → Charger: Connect the LiPo to the charger BAT terminals.
2) Charger → 5V boost: Feed the charger’s OUT (or battery rail) to the boost converter input.
3) Boost 5V → Power Switch IN: Connect boost +5V to the high‑side switch VIN; share grounds.
4) Power Switch OUT → USB VBUS: Connect switch VOUT to USB‑A receptacle VBUS (red wire pin). Connect grounds together.
5) EN control → XIAO D16 (GPIO10): Wire the switch EN pin to D16. The firmware drives it HIGH to enable 5V to the calculator.
6) Optional FAULT → XIAO GPIO: If your switch exposes FAULT, wire it to a spare GPIO with a pull‑up to 3.3V to monitor over‑current.
7) USB data lines to ESP32S3 device port are not used here; the ESP32 acts as USB host via its OTG port. Use a proper OTG/host adapter from the ESP32’s USB to your receptacle/cable path if required by your mechanical design.

USB‑A pinout reminder
- VBUS: +5V
- D− / D+: USB data (not routed through the power switch)
- GND: Ground

If using USB‑C receptacle for the calculator
- Add 5.1 kΩ pull‑ups (Rp) from CC1 and CC2 to +5V to advertise as a 5V source.
- Only one CC needs to be populated depending on plug orientation, but fitting both is typical.

Bring‑up checklist
- With the ESP32 unpowered and the switch disabled (EN low), verify no 5V is present at the receptacle.
- Power the board from battery; measure boost output at 5V and switch VIN.
- Program firmware; confirm Serial shows Wi‑Fi connected and “USB Host: ready”.
- EN goes HIGH after init; measure 5V at receptacle VBUS.
- Plug the calculator; Serial should print NEW_DEV and a VID/PID.

## Complete Build & Test Guide

### Step 1: Set up the Server

1. **Install Prerequisites**:
   - Install [Node.js LTS](https://nodejs.org/)
   - Install [Ollama](https://ollama.ai/)

2. **Configure Ollama**:
   ```powershell
   # Pull DeepSeek-R1 model (or your preferred model)
   ollama pull deepseek-r1:latest
   
   # Start Ollama (if not running)
   ollama serve
   ```

3. **Configure & Start Server**:
   ```powershell
   # From project root
   cd server
   
   # Copy and edit .env file
   copy .env.example .env
   # Edit .env if needed (default settings work for local Ollama)
   
   # Install dependencies
   npm install
   
   # Start server
   npm start
   ```

4. **Verify Server**:
   ```powershell
   # In another terminal
   curl http://localhost:8080/health
   # Should return: {"ok":true,"model":"deepseek-r1:latest"}
   
   # Test the AI endpoint
   curl "http://localhost:8080/gpt/ask?question=What+is+2+plus+2"
   ```

### 2. Flash ESP32-S3 Firmware (XIAO ESP32S3)

**📖 See [XIAO_ESP32S3_GUIDE.md](XIAO_ESP32S3_GUIDE.md) for detailed XIAO-specific instructions**

1. **Configure WiFi & Server**:
   - Edit `esp32/secrets.h`
   - Set your WiFi SSID and password
   - Set `SERVER` to your computer's IP (e.g., `"http://192.168.1.100:8080"`)
   
   Find your IP:
   ```powershell
   ipconfig
   # Look for "IPv4 Address" under your WiFi adapter
   ```

2. **Arduino IDE Setup for XIAO**:
   - Install ESP32 board support (2.0.14+)
   - Select board: **Tools → Board → esp32 → XIAO_ESP32S3**
   - Set **USB CDC On Boot: Enabled**
   - Set **USB Mode: Hardware CDC and JTAG**
   
3. **Flash Firmware**:
   - Open `esp32s3/esp32s3_host.ino` in Arduino IDE
   - Select your XIAO's COM port
   - Click Upload
   
3. **Verify Connection**:
   - Open Serial Monitor (115200 baud)
   - You should see:
     ```
     [S3 Host] Boot
     WiFi: connected
     IP: 192.168.x.x
     USB Host: ready
     VBUS: enabled on GPIO10
     ```

### Step 3: Program the Calculator

1. **Transfer Python Program**:
   - Connect TI-84 Plus CE to PC via USB
   - Use TI Connect CE software
   - Transfer `ti84/ti32_ai.py` to calculator
   - Optionally transfer `ti84/simple_test.py` for testing

2. **Initial Test**:
   - Disconnect calculator from PC
   - Connect calculator to ESP32-S3 via USB OTG cable
   - On calculator, run `simple_test` program
   - Should see: "What is 2+2?" → "4" (or similar)

3. **Run Full Program**:
   - On calculator, run `ti32_ai` program
   - Follow menu to ask questions

### Step 4: Hardware Assembly (XIAO ESP32S3 Power Circuit)

**⚠️ Critical**: XIAO ESP32S3 does NOT provide 5V VBUS - you must add external power circuit!

**Quick breadboard setup** (for testing):
- External 5V source → Calculator VBUS
- Common ground between XIAO and 5V source
- XIAO USB D+/D- → Calculator D+/D-

**Battery-powered setup** (recommended):
1. See **[XIAO_ESP32S3_GUIDE.md](XIAO_ESP32S3_GUIDE.md)** for wiring diagrams
2. Follow `pcb/README.md` for complete PCB design
3. Key components:
   - 5V boost converter (MT3608 or TPS61023)
   - High-side switch (AP2331 or TPS2553) with EN on GPIO10/D16
   - USB-A receptacle for calculator
   - LiPo battery (3.7V, 1000-2000mAh)
   - XIAO has built-in battery charging!

## Troubleshooting

### Server Issues

**"Error connecting to Ollama"**
- Ensure Ollama is running: `ollama serve`
- Check Ollama URL in `.env` file
- Test directly: `curl http://localhost:11434/api/tags`

**"bad port" error**
- Check PORT in `.env` (should be 8080)
- Ensure port is not already in use

### ESP32 Issues

**WiFi not connecting**
- Double-check SSID and password in `secrets.h`
- Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Check WiFi signal strength

**"USB Host: failed"**
- Ensure you're using ESP32-S3 (not ESP32)
- Check board selection in Arduino IDE
- Verify USB host pins are correct for your board

**Calculator not detected**
- Check USB cable connection
- Verify 5V VBUS is present (GPIO10 HIGH)
- Check Serial Monitor for "NEW_DEV" message

### Calculator Issues

**"Timeout waiting for response"**
- Check ESP32 Serial Monitor for errors
- Verify WiFi and server connectivity
- Ensure calculator USB cable is connected

**"ti_system not available"**
- This is normal when testing on PC
- Program must run on actual TI-84 Plus CE Python Edition

## Quick Test Sequence

```powershell
# Terminal 1: Start Ollama
ollama serve

# Terminal 2: Start server
cd server
npm start

# Terminal 3: Monitor ESP32
# Open Arduino IDE → Serial Monitor (115200 baud)
# Watch for WiFi connection and USB events

# Calculator: Run simple_test.py
# Should see question and answer cycle
```

## License

See [LICENSE](./LICENSE).
