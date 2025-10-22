# XIAO ESP32S3 Setup Guide for TI-32

Complete guide for using the Seeed Studio XIAO ESP32S3 with this project.

## Why XIAO ESP32S3?

The XIAO ESP32S3 is an excellent choice for this project:
- ✅ Compact size (21mm x 17.5mm)
- ✅ Built-in WiFi & Bluetooth
- ✅ USB OTG (host mode) support
- ✅ Battery charging circuit (for portable use)
- ✅ Low power consumption
- ✅ Arduino IDE compatible

## Important: VBUS Power Requirement

⚠️ **Critical**: The XIAO ESP32S3's USB-C port does NOT provide 5V output by default. You **must** add an external circuit to provide 5V VBUS to the calculator.

## Hardware Setup Options

### Option 1: Breadboard Testing (Simplest)

**Parts needed:**
- XIAO ESP32S3
- 5V power supply or USB power bank with USB breakout
- Breadboard and jumper wires
- USB-A breakout board or cable

**Wiring:**
```
External 5V Source → USB-A VBUS (calculator power)
XIAO GND ← USB-A GND (common ground)
XIAO D+ (USB) → Calculator D+
XIAO D- (USB) → Calculator D-
```

**Note**: For testing, you can manually provide 5V to the calculator's VBUS without GPIO control. Just ensure the 5V source can provide at least 500mA.

### Option 2: Battery Powered with Switch (Recommended)

**Parts needed:**
- XIAO ESP32S3
- 3.7V LiPo battery (1000-2000mAh)
- 5V boost converter (MT3608 or TPS61023)
- High-side power switch (AP2331 or TPS2553)
- USB-A receptacle
- Perfboard or PCB

**Wiring diagram:**
```
Battery (+) → Boost VIN
Battery (-) → GND (common)
Boost VOUT (+5V) → Switch VIN
Switch VOUT → USB-A VBUS
Switch EN → XIAO D16 (GPIO10)
USB-A GND → GND (common)
XIAO USB D+/D- → USB-A D+/D-
```

**Component recommendations:**
- **Boost converter**: MT3608 module (~$2, 2A capable)
- **Power switch**: AP2331W-7 (SOT-23-5, integrated current limit)
- **Alternative switch**: TPS2553DBVR (more features, programmable limit)

### Option 3: Custom PCB (Most Professional)

Follow the complete PCB design guide in `pcb/README.md` which includes:
- Full schematic for XIAO ESP32S3 integration
- Battery charging circuit
- 5V boost converter
- High-side switch with GPIO control
- USB receptacle for calculator
- ESD protection
- Compact form factor

## Arduino IDE Setup

### 1. Install Board Support

1. Open Arduino IDE
2. Go to **File → Preferences**
3. Add to "Additional Board Manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Go to **Tools → Board → Boards Manager**
5. Search for "esp32"
6. Install "esp32" by Espressif Systems (version 2.0.14 or newer)

### 2. Select Board

1. **Tools → Board → esp32 → XIAO_ESP32S3**
2. **Tools → USB CDC On Boot → Enabled** (important for Serial output)
3. **Tools → USB Mode → Hardware CDC and JTAG**
4. **Tools → Port → Select your XIAO's COM port**

### 3. Upload Settings

- **Upload Speed**: 921600 (or 115200 if you have upload issues)
- **CPU Frequency**: 240MHz (default)
- **Flash Size**: 8MB (default for XIAO ESP32S3)
- **Partition Scheme**: Default 4MB with spiffs

## Pin Reference for XIAO ESP32S3

```
XIAO Label → GPIO → Function in this project
─────────────────────────────────────────────
D0         → GPIO1  → Available
D1         → GPIO2  → Available
D2         → GPIO3  → Available
D3         → GPIO4  → Available
D4         → GPIO5  → Available
D5         → GPIO6  → Available
D6         → GPIO43 → Available
D7         → GPIO44 → Available
D8         → GPIO7  → Available
D9         → GPIO8  → Available
D10        → GPIO9  → Available

D16        → GPIO10 → VBUS_EN (5V switch control) ⚡
USB D+/D-  → Native → USB Host to calculator
BAT+       → Battery input (3.7V LiPo)
5V         → 5V input (USB power in, not output)
3V3        → 3.3V output (max 700mA)
GND        → Ground (common with calculator)
```

## Flashing the Firmware

### Step 1: Configure WiFi & Server

Edit `esp32/secrets.h`:
```cpp
#define WIFI_SSID "YourWiFiNetwork"
#define WIFI_PASS "YourPassword"
#define SERVER "http://192.168.1.XXX:8080"  // Your PC's IP
```

Find your PC's IP:
- Windows: `ipconfig` (look for IPv4 Address)
- Mac: `ifconfig en0 | grep inet`
- Linux: `ip addr show`

### Step 2: Compile & Upload

1. Open `esp32s3/esp32s3_host.ino` in Arduino IDE
2. Select XIAO_ESP32S3 board (see Arduino IDE Setup above)
3. Click **Verify** (checkmark) to compile
4. Click **Upload** (arrow) to flash

### Step 3: Monitor Serial Output

1. Open **Tools → Serial Monitor**
2. Set baud rate to **115200**
3. You should see:
   ```
   [S3 Host] Boot
   WiFi: connected
   IP: 192.168.x.x
   USB Host: ready
   VBUS: enabled on GPIO10
   ```

## USB Host Mode Configuration

The XIAO ESP32S3 has a single USB-C port that can function as either:
- **Device mode** (default): For programming and USB serial
- **Host mode**: For connecting to the calculator

The firmware automatically configures USB host mode after boot. The USB-C port is used for:
1. **Programming**: USB device mode (before firmware runs)
2. **Calculator connection**: USB host mode (when firmware is running)

### Important USB Notes:

1. **You cannot use USB serial and calculator simultaneously** on the single USB port
2. **For debugging while calculator is connected**: Use WiFi logging or LED indicators
3. **The XIAO's USB-C port does not provide 5V VBUS** - you must supply it externally

## Testing Without Calculator

Before connecting the calculator, verify the system works:

```cpp
// Add to setup() for LED feedback (if you have one on GPIO1)
pinMode(1, OUTPUT);

// In loop(), blink to show it's alive
digitalWrite(1, !digitalRead(1));
delay(500);
```

Or use WiFi debugging:
```cpp
// Add after WiFi connects
Serial.println(WiFi.localIP());  // Check this in Serial Monitor before connecting calculator
```

## Power Supply Considerations

### USB Power (for development)
- Connect XIAO to PC via USB-C
- **WARNING**: Do not backfeed 5V from your external circuit to PC USB
- Use diodes or only enable VBUS when on battery power

### Battery Power (for portable use)
- Use 3.7V LiPo (500-2000mAh)
- XIAO has built-in charging (when connected to USB)
- Boost to 5V for calculator VBUS (500mA minimum)
- Run time: ~4-8 hours depending on battery size and usage

### Power Budget
- XIAO ESP32S3: ~100-200mA (WiFi active)
- Calculator: ~100-300mA (typical)
- Boost converter losses: ~20%
- **Total from battery**: ~300-600mA at 3.7V

## Troubleshooting XIAO ESP32S3

### Upload Issues

**"Failed to connect to ESP32"**
1. Hold BOOT button while clicking RESET
2. Click Upload while holding BOOT
3. Release after upload starts

**Wrong port selected**
- Windows: Check Device Manager → Ports (COM & LPT)
- Mac/Linux: `ls /dev/tty.*` or `ls /dev/ttyACM*`

### WiFi Issues

**WiFi not connecting**
- XIAO only supports 2.4GHz WiFi (not 5GHz)
- Check SSID and password in `secrets.h`
- Some ESP32S3 boards need antenna configuration (XIAO has built-in)

### USB Host Issues

**"USB Host: failed"**
- Ensure board is selected as XIAO_ESP32S3 (not generic ESP32)
- Check USB CDC settings in Arduino IDE
- Try different USB cable (some are power-only)

**Calculator not detected**
- Verify 5V VBUS is present (use multimeter)
- Check GPIO10 is HIGH (use Serial Monitor or LED)
- Try toggling VBUS: disconnect calculator, restart XIAO, reconnect

### Serial Monitor Issues

**No output**
- Ensure "USB CDC On Boot" is **Enabled** in Tools menu
- Try 115200 baud rate
- After calculator connects, Serial may not work (USB is in host mode)

## Quick Start Checklist for XIAO ESP32S3

- [ ] Arduino IDE installed with ESP32 board support
- [ ] Board selected: XIAO_ESP32S3
- [ ] USB CDC On Boot: Enabled
- [ ] secrets.h configured with WiFi and server IP
- [ ] Firmware compiled successfully
- [ ] 5V VBUS circuit wired (boost converter + switch)
- [ ] Serial Monitor shows WiFi connected
- [ ] GPIO10 (D16) wired to switch EN pin
- [ ] USB data lines (D+/D-) connected to calculator
- [ ] Calculator connects and shows "NEW_DEV" in logs

## Next Steps

1. **Test basic connectivity**: Follow steps above
2. **Build power circuit**: Use Option 2 wiring or custom PCB
3. **Transfer calculator program**: See `ti84/README.md`
4. **Run end-to-end test**: Ask AI a question from calculator!

## Resources

- [XIAO ESP32S3 Wiki](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
- [XIAO ESP32S3 Pinout](https://files.seeedstudio.com/wiki/SeeedStudio-XIAO-ESP32S3/img/2.jpg)
- [Arduino-ESP32 USB Host Examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/USB/examples)
- Project PCB Guide: `../pcb/README.md`

---

**Ready to build!** The XIAO ESP32S3 is perfect for this project - compact, capable, and battery-friendly. 🚀
