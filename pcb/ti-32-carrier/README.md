# KiCad Starter Project: TI-32 Carrier

Open `ti-32-carrier.kicad_pro` in KiCad 8. This starter includes:
- A blank but annotated schematic sheet with block notes and nets reference
- An empty board with a 60 x 35 mm outline (Edge.Cuts) and a title

Recommended next steps
1. Place symbols (or replace with your part choices):
   - Connector: `Connector:USB_A` (or use `Connector:USB_C_Receptacle_USB2.0` + CC pull-ups)
   - ESD: `Power_Protection:USBLC6-2SC6`
   - Switch: `Power_Switch:AP2331` (or `TPS2553`) – if missing, use a generic `Regulator_Switch` symbol and update later
   - Boost: `Regulator_Switching` family (e.g., `MT3608` or `TPS61023` symbol from community libs) – you can start with a generic boost symbol
   - Battery: `Connector_Generic:Conn_01x02` for JST-PH
   - XIAO: use two 1x7/1x8 headers to represent the module pins or add a custom symbol later
2. Label nets:
   - `BAT`, `+5V`, `VBUS`, `D+`, `D-`, `EN_VBUS`
   - Tie `EN_VBUS` to XIAO pin `GPIO10 (D16)` by net label
3. Run ERC, then Assign Footprints:
   - USB-A receptacle footprint (vertical or right-angle)
   - ESD diode SOT-23-6
   - AP2331 SOT-23-5 (or TPS2553 MSOP-8/SON)
   - Boost (MT3608 SOT-23-6 or your chosen IC)
   - JST-PH-2 for battery
4. Update PCB from schematic and place parts:
   - Keep D+/D- short, matched, away from the boost loop
   - ESD next to the connector
   - Switch close to connector; 10–22 µF bulk cap on VBUS
   - Observe XIAO antenna keep-out near the USB end of the module

Notes
- The firmware defaults EN_VBUS to GPIO10 (D16). If you pick another pin, update `PIN_VBUS_EN` in `esp32s3/esp32s3_host.ino`.
- For USB-C, add 5.1 kΩ `Rp` on CC1/CC2 to +5V to advertise a 5V source.
