# DeepSeek-TI-84  (calculator ↔ AI)
A TI-84 Plus CE Python asks DeepSeek-R1 questions over a $7 ESP32-S3 dongle.

## 1. What you need to buy (exact parts)
| Qty | Part | Footprint | Mouser | LCSC | Notes |
|----:|------|-----------|--------|------|-------|
| 1   | XIAO ESP32-S3 | dev-board | 356-ESP32S3XIAO | C558488 | already has USB-C |
| 1   | USB-C receptacle 6-pin mid-mount | - | 798-006-0101-1 | C134092 | board edge |
| 2   | 5.1 kΩ 0603 | 0603 | - | C23187 | CC1/CC2 pull-downs |
| 2   | 100 nF 0603 | 0603 | - | C14663 | decoupling |
| 1   | PCB | 50 × 25 mm | order gerber.zip below | JLCJLCJLC | $4 for 5 pcs |

Total ≈ US $7 + PCB.

## 2. Order the PCB (2 min)
1. Download [pcb/gerber.zip](pcb/gerber.zip)  
2. Upload to [jlcpcb.com](https://jlcpcb.com), 2-layer, 1 mm, black, 1 oz Cu.  
3. Quantity 5, shipping as you like → ≈ $4 delivered.

## 3. Flash the ESP32-S3 (30 s)
```bash
# Linux/macOS/WSL
python flash_firmware.py   # auto-downloads UF2, puts XIAO in DFU, flashes