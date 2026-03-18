# 3D Print Files — FluidLevelMonitor Sensor Enclosure

## Files

| File | Description |
|------|-------------|
| `sensor_enclosure.scad` | Master OpenSCAD file — all 5 parts |

## How to export each part for slicing

Open `sensor_enclosure.scad` in OpenSCAD, change the `PART = "..."` line at the bottom:

| PART value | What it exports | Print orientation |
|---|---|---|
| `"body"` | Main enclosure box | Open face up — no supports needed |
| `"lid"` | Snap+screw lid | Outside face down (flat) — supports for gasket groove |
| `"pcb_tray"` | Snap-in PCB tray | Any — no supports needed |
| `"battery_cradle"` | 2× 32700 cell holder | Open face up — no supports needed |
| `"sensor_bracket"` | US-100 tank mount bracket | Flat face down — no supports |
| `"all_exploded"` | All parts side-by-side | For checking fit only — not for slicing |

Then: **Render** (F6) → **Export as STL** (File → Export → Export as STL).

## Print settings (Bambu Lab K1C)

| Setting | Value | Reason |
|---------|-------|--------|
| **Material** | **ASA** | UV and heat resistant. Outdoor enclosures in India need ASA — PLA warps above 60°C, PETG above 80°C |
| Layer height | 0.2mm | Good balance of strength and speed |
| Wall loops | **4** | Minimum for weatherproofing — 4 × ~0.86mm ≈ 3.4mm solid walls |
| Top/bottom layers | 5 | Solid top face is critical for lid seal |
| Infill | 40% gyroid | Strong in all directions |
| Supports | Auto (normal) | Only needed for body's cable gland boss and lid gasket groove |
| Brim | 5mm | ASA warps — use brim or glue stick on plate |
| Print temp | 240°C (nozzle) / 90°C (bed) | Standard ASA profile on K1C |
| Enclosure | **Closed** | ASA requires enclosed chamber — K1C is enclosed ✅ |
| AMS | Single colour | No colour change needed |

## Parts overview

```
                   ┌─────────────────────┐
                   │    ENCLOSURE LID    │  ← M3 screws × 4
                   │  (USB-C cutout)     │
         ──────────┤─────────────────────├──────────
         │         │    PCB TRAY         │          │
         │         │  (D1 Mini + buck    │          │
BODY     │         │   + charger)        │          │
115×     │         └──────────────────── │          │
89×      │         ┌─────────────────────┐          │
55mm     │         │  BATTERY CRADLE     │          │
         │         │  (2× 32700 cells)   │          │
         │─────────┴─────────────────────┴──────────│
         │  ← cable gland hole (12mm) on bottom     │
         └───────────────────────────────────────────┘
                   
                         ↕ separate part
                         
         ┌─────────────────────────────────────────┐
         │            SENSOR BRACKET               │
         │  (mounts US-100 over tank opening)      │
         │  Cable ties + M4 mounting holes         │
         └─────────────────────────────────────────┘
```

## Post-print finishing

1. **Body + Lid:** Sand the lid mating face with 400-grit to ensure flat seal surface
2. **Gasket groove:** Press 2mm × 2mm square EPDM cord (cut to length, join with superglue) into the groove
3. **Cable gland hole:** Thread a PG7 cable gland (hand tight + 1/4 turn with wrench). Apply RTV silicone from inside
4. **USB-C cutout:** Install panel-mount USB-C connector, seal perimeter with RTV
5. **ASA post-process:** Light coat of UV-resistant clear lacquer extends UV life significantly

## Key dimensions (for verification after print)

Measure these with calipers before assembly:

| Dimension | Target | Tolerance |
|-----------|--------|-----------|
| Body inner width | 115mm | ±0.5mm |
| Body inner height | 89mm | ±0.5mm |
| Body inner depth | 53mm | ±0.5mm |
| Cell pocket diameter | 32.5mm | ±0.3mm (should be snug) |
| Cable gland hole | 12.2mm | ±0.1mm |
| USB-C cutout | 10.0 × 7.5mm | ±0.2mm |
| Boss hole (M3) | 2.8mm | ±0.1mm |
| Lid rim fit | Snug slide, no rattle | — |
