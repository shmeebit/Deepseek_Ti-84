# TI-32 Carrier PCB Guide (Seeed Studio XIAO ESP32S3 → TI‑84 CE over USB Host)

This guide describes a compact carrier PCB specifically designed for the **Seeed Studio XIAO ESP32S3**. It includes the battery power path, 5V VBUS generation, high-side switch control, and a receptacle for the calculator. 

**Why XIAO ESP32S3?**
- Ultra-compact (21mm x 17.5mm)
- Built-in battery charging
- Native USB OTG support
- Perfect for portable calculator AI assistant!

## Functional blocks

- U1: **Seeed Studio XIAO ESP32S3** (module). Keep clear area for antenna (top edge of board).
- J1: USB receptacle for the calculator
  - Option A: USB‑A female (simplest)
  - Option B: USB‑C female with 5.1 kΩ CC pull‑ups (source‑only)
- U2: 5V boost converter from single‑cell LiPo (≥ 1 A out)
  - Examples: TPS61023, MP3426, MT3608 (budget), or similar
- U3: High‑side USB power switch with current limit
  - Examples: AP2331 (0.9–1.2 A limit), TPS2553 (programmable limit)
- D1: USB ESD protection (low capacitance, 2‑line)
  - Example: USBLC6‑2SC6, ESD9M5V
- J2: Battery connector (if not using the XIAO’s built‑in battery input)
  - JST‑PH‑2 or solder pads to the module BAT/GND
- LED1 + Rled: VBUS present indicator (optional)
- TPx: Test pads (5V, VBUS, BAT, GND, EN)

## Schematic outline

Net naming (suggested):
- BAT: Battery rail from LiPo (≈3.0–4.2 V)
- +5V: Boost output
- VBUS: Switched 5V to the calculator receptacle
- D+ / D−: USB data pair from the ESP32S3 to J1
- EN_VBUS: Switch enable (GPIO10/D16 default)

Connections
1) Battery → U2 boost
   - BAT → U2 VIN, GND common
   - U2 VOUT → +5V net, local 22–47 µF output cap near U2
2) +5V → U3 power switch
   - +5V → U3 IN
   - U3 OUT → VBUS net (to J1)
   - EN_VBUS (GPIO10) → U3 EN (add 100 kΩ pulldown to keep off at reset)
   - Current limit set per datasheet (e.g., ILIM resistor for TPS2553)
   - Place 10 µF+ bulk cap on VBUS near J1
3) USB receptacle J1
   - If USB‑A: route VBUS/GND and connect D+/D− pair
   - If USB‑C: add 5.1 kΩ to +5V for CC1 and CC2 (Rp) to advertise as a 5V source only
   - Place D1 (ESD) close to J1 on D+/D− to GND
4) XIAO ESP32S3 Plus module
   - Use pins D+ / D− pads from the module to your D+/D− nets
   - EN_VBUS from GPIO10 (D16) to U3 EN
   - BAT and GND as needed if you power boost from the module’s BAT rail

Reference parts
- U3 AP2331 (SOT‑23‑5) or TPS2553 (MSOP‑8/SON), both have FAULT pins for over‑current indication
- D1 USBLC6‑2 (SOT‑23‑6) near connector
- U2 choose per availability; TPS61023 (QFN) is compact/efficient; MT3608 (SOT‑23‑6) is easy, less efficient

## Layout guidance

- Keep the D+/D− differential pair short, matched, and away from the boost/switch power loops. Target 90 Ω diff if possible; for full‑speed 12 MHz the routing is tolerant but symmetry and length match help.
- Place the ESD diode next to the receptacle; route D+/D− through it first.
- Put the high‑side switch close to the receptacle; keep the VBUS decoupling (10–22 µF) next to J1.
- Keep the boost converter loop tight: input cap → IC → inductor → diode/switch → output cap. Short traces, wide pours.
- Use a solid ground plane; stitch vias liberally. Keep analog ground of the boost under the IC and away from D+/D− if you can.
- Respect the XIAO’s antenna keep‑out (top edge). Avoid copper and tall parts near it.
- Add test pads: TP_BAT, TP_5V, TP_VBUS, TP_EN, TP_GND for bring‑up.

## Protections and options

- Add a Schottky from +5V to VBUS if you want a measurement tap before the switch (usually not needed).
- Use the switch FAULT pin to an ESP32 GPIO with a 100 kΩ pull‑up to 3.3V to detect over‑current and disable EN in firmware.
- Consider a P‑MOS ideal‑diode or a power mux if you plan to connect PC USB to the XIAO while also sourcing VBUS to the calculator; otherwise, ensure firmware disables EN when PC USB is attached.

## Bill of materials (example)

- U1: Seeed XIAO ESP32S3 Plus (1x)
- J1: USB‑A vertical or right‑angle (1x) OR USB‑C female (1x) + 5.1 kΩ 1% (2x) for CC1/CC2
- U2: TPS61023 or MT3608 boost converter (1x)
- Inductor per U2 datasheet (e.g., 2.2–4.7 µH, ≥2 A sat current)
- U3: AP2331 (SOT‑23‑5) or TPS2553 (MSOP‑8/SON) (1x)
- D1: USBLC6‑2SC6 ESD diode (1x)
- Cbulk: 10–22 µF on VBUS near J1 (1–2x), electrolytic or low‑ESR ceramic
- Cin/Cout for U2 per datasheet (2–3x)
- R_EN_PD: 100 kΩ pulldown on EN (1x)
- LED1: 0603 LED + 1 kΩ (optional)
- J2: JST‑PH‑2 battery connector (optional if not using the XIAO’s BAT pads)

## Schematic checklist

- [ ] BAT to boost VIN, grounds common
- [ ] +5V to switch IN; switch OUT to J1 VBUS
- [ ] EN from GPIO10 with PD to GND
- [ ] ESD on D+/D− at connector; route to XIAO D+/D−
- [ ] If USB‑C, CC1/CC2 5.1 kΩ to +5V
- [ ] Bulk caps at switch OUT and J1
- [ ] Test pads present

## Bring‑up procedure

1) Don’t populate EN link (or keep EN pulled LOW). Power from battery. Verify +5V present at boost output, not at VBUS.
2) Program firmware; confirm Wi‑Fi and USB host ready. EN toggles HIGH automatically and VBUS becomes 5V.
3) Plug the calculator; watch Serial for NEW_DEV with VID/PID and discovered bulk endpoints.
4) If over‑current trips, FAULT goes low; disable EN and inspect load.

## Form factor suggestions

- “Stick” board with XIAO on top edge (antenna free), USB‑A on opposite edge. Boost + switch mid‑board, short VBUS path.
- Or a small “hat” for XIAO with side‑mount USB‑A and JST battery at back. Keep the USB pair away from the boost inductor.

---

If you’d like, I can generate a KiCad starter project with these blocks and footprints wired to your chosen pin (GPIO10 for EN) so you can iterate layout quickly.
