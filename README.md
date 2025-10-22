# TI‑32 (ESP32‑S3 USB Host + Ollama DeepSeek‑R1)

Portable calculator ↔ ESP32‑S3 bridge to a local LLM via Ollama.

- TI‑84 Plus CE Python connects over USB to an ESP32‑S3 acting as a USB host.
- The ESP connects over Wi‑Fi to a Node.js server that proxies to your local Ollama model (DeepSeek‑R1 by default).

## Repo layout

- esp32s3/ – ESP32‑S3 host firmware (USB host + Wi‑Fi bridge)
- esp32/ – Shared config: `secrets.h` (Wi‑Fi/server)
- server/ – Minimal Node/Express bridge to Ollama (`/gpt` only)
- pcb/ – Carrier PCB guide (schematic blocks, BOM, layout)

## Quick start

1) Server + Ollama
   - Install Ollama and pull a DeepSeek‑R1 variant (e.g., `deepseek-r1:latest`).
   - In `server/`, copy `.env.example` to `.env` and set `OLLAMA_URL` and `OLLAMA_MODEL`.
   - Start the server. It exposes `/gpt` and helper endpoints.
2) ESP32‑S3 firmware
   - Edit `esp32/secrets.h` with your Wi‑Fi and server base URL.
   - Build and flash `esp32s3/esp32s3_host.ino` on an ESP32‑S3 board with USB host capability.
   - XIAO ESP32S3 Plus: provide 5V VBUS via an external 5V source and a high‑side switch (e.g., AP2331/TPS2553) controlled by a GPIO.
   - Open Serial Monitor (115200) to confirm Wi‑Fi connects and USB Host installs.
3) Connect the calculator
   - Use a USB‑C OTG/host adapter into the ESP’s host port.
   - Ensure 5V VBUS is sourced to the USB port; otherwise the calculator won’t enumerate.

Status
- Server is ready for local use with Ollama.
- ESP32‑S3 firmware: Wi‑Fi + USB host driver scaffold in place; TI USB link layer (Str/Real transfers) is next.

Notes
- Use a USB‑C OTG/host adapter to ensure the ESP32‑S3 takes host role; it does not supply 5V by itself.
- Assert VBUS (enable your switch) after Wi‑Fi is up, then the USB host driver will enumerate the calculator when plugged.

## What's left to build

- ESP32‑S3 firmware
   - USB attach handling: host events, NEW_DEV detection, device + active config descriptors are read and logged.
   - Next: Parse configuration/interface descriptors and open the vendor/bulk IN/OUT endpoints used by the TI link protocol.
   - Next: Implement silent transfers for StrN and Real variables (minimal subset; enough for prompt in/out).
   - VBUS control added via GPIO10 (D16) on XIAO ESP32S3 Plus (changeable via `PIN_VBUS_EN`).
- Server
   - Optional: add streaming responses for longer answers.
   - Optional: add a tiny auth header check for LAN locking.

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

## Run the server on Windows

1) Install Node.js (LTS) and ensure `node` is on PATH.
2) Configure Ollama and pull a model (default is `deepseek-r1:latest`).
3) In `server/`:

```powershell
# from project root
Set-Location .\server
npm install
npm start
# optional: check
curl http://localhost:8080/health
```

## License

See [LICENSE](./LICENSE).
