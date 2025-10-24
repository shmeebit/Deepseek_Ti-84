# Hardware Setup Guide for Deepseek_Ti-84

## Required Components

### Core Components
- **Seeed Studio XIAO ESP32S3** - Main microcontroller
- **TI-84 Plus CE Python** - Calculator with Python support
- **LiPo Battery** - 3.7V, 1000-2000mAh capacity
- **USB Cable** - USB-A to USB-C for calculator connection

### Power Circuit Components
- **5V Boost Converter** - MT3608 or similar (≥1A output)
- **High-side USB Power Switch** - AP2331 or TPS2553 with current limiting
- **USB-A Receptacle** - For calculator connection
- **Various Resistors** - 5.1kΩ for USB-C CC pins (if using USB-C)
- **Capacitors** - 10µF and 100nF for power stability

## Wiring Diagram
Battery (3.7V LiPo)
│
├──→ [Boost Converter] → 5V
│                       │
│                       └──→ [Power Switch IN]
│                                        │
│                                        └──→ [Power Switch OUT] → [USB VBUS] → Calculator
│                                        │
│                       [Control GPIO10] ←─── XIAO ESP32S3 (D16)
│
└───────────────────────────────────────────→ Common Ground


## Connection Details

### Power Circuit
1. **Battery → Boost Converter**
   - Connect LiPo positive to boost converter VIN
   - Connect LiPo negative to common ground

2. **Boost Converter → Power Switch**
   - Boost 5V output to power switch VIN
   - Common ground connection

3. **Power Switch → USB Port**
   - Power switch VOUT to USB receptacle VBUS (pin 1)
   - USB receptacle ground to common ground

4. **ESP32 Control**
   - XIAO GPIO10 (D16) to power switch EN pin
   - Add pull-down resistor (10kΩ) to ensure switch defaults to OFF

### USB Data Connections
- **XIAO ESP32S3 USB D+** → USB receptacle D+ (pin 3)
- **XIAO ESP32S3 USB D-** → USB receptacle D- (pin 2)

### USB-C Specific (if using USB-C receptacle)
- Add 5.1kΩ pull-up resistors from CC1 and CC2 to 5V
- This advertises the ESP32 as a 5V power source

## Assembly Steps

### 1. Power Circuit Assembly
1. Mount boost converter on breadboard/PCB
2. Connect LiPo battery connector
3. Add input and output capacitors
4. Test boost converter output (should be 5V)

### 2. Power Switch Integration
1. Add high-side power switch after boost converter
2. Connect EN pin to XIAO GPIO10
3. Add current limiting resistor if needed
4. Test power switch control from ESP32

### 3. USB Port Connection
1. Mount USB receptacle
2. Connect VBUS, D+, D-, and ground
3. Add USB-C pull-ups if using USB-C
4. Test with multimeter for proper connections

### 4. Final Testing
1. Power on ESP32 (battery or USB)
2. Verify 5V appears on USB VBUS when GPIO10 is HIGH
3. Connect calculator and verify it powers on
4. Check ESP32 serial output for calculator detection

## Safety Considerations

### Power Safety
- **Current Limiting**: Ensure power switch limits current to 500mA
- **Over-current Protection**: Use power switch with built-in protection
- **Reverse Protection**: Consider adding reverse polarity protection
- **Thermal Protection**: Use components with thermal shutdown

### USB Safety
- **Back-powering Prevention**: Never connect to PC USB while VBUS is active
- **Power Path Control**: Use diodes or switches to prevent conflicts
- **ESD Protection**: Add ESD protection diodes on USB lines

### Battery Safety
- **Protection Circuit**: Use LiPo with built-in protection
- **Charging**: Never charge battery while connected to calculator
- **Monitoring**: Consider adding battery voltage monitoring

## Troubleshooting

### Power Issues
- **No 5V Output**: Check boost converter connections and adjustment
- **Calculator Won't Power**: Verify VBUS is actually 5V under load
- **ESP32 Resetting**: Add more capacitance or check power supply capacity

### USB Issues
- **Calculator Not Detected**: Check USB data line connections
- **Intermittent Connection**: Add pull-up resistors or check cable quality
- **Enumeration Fails**: Ensure proper timing of VBUS enable

### Communication Issues
- **No Response**: Check ESP32 serial output for errors
- **Timeout Errors**: Verify WiFi connection and server availability
- **Protocol Errors**: Check for proper TI calculator model compatibility

## Bill of Materials

| Component | Part Number | Quantity | Estimated Cost |
|-----------|-------------|----------|----------------|
| XIAO ESP32S3 | Seeed Studio | 1 | $10-15 |
| LiPo Battery | 3.7V 1000mAh | 1 | $5-10 |
| Boost Converter | MT3608 | 1 | $2-3 |
| Power Switch | AP2331 | 1 | $1-2 |
| USB Receptacle | USB-A Female | 1 | $1-2 |
| Resistors | Various | 5 | $1 |
| Capacitors | Various | 4 | $1 |
| **Total** | | | **$20-35** |

## Additional Resources

- [XIAO ESP32S3 Pinout](https://wiki.seeedstudio.com/XIAO_ESP32S3/)
- [TI-84 Plus CE Python Guide](https://education.ti.com/en/guidebook/details/en/14145)
- [ESP32-S3 USB Host Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/usb_host.html)