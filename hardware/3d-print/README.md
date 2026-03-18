# 3D Print Files — FluidLevelMonitor Sensor Node

Three SCAD files. Open in OpenSCAD (or openscad.cloud), set `PART = "..."`, press F6, export STL.

---

## Files and parts

### 1. `sensor_mount_sintex.scad` — for Sintex overhead tank (black dome lid)

| PART value | What it is |
|---|---|
| `"mount"` | Main hub + arms that clamp onto the Sintex lid |
| `"clamp"` | Ring that goes under the lid to lock mount in place |

**How it mounts:** Boss drops through the ~40mm central hole in the Sintex dome lid. Three arms rest on the lid surface. The clamp ring slides up the boss from inside, M3 set-screws tighten. US-100 PCB slides into pocket inside hub, faces downward into tank. 4-wire cable exits via PG7 gland on hub side.

**Parameter to adjust:** `BOSS_OD = 38.5` — measure your tank's lid hole diameter and subtract 0.5mm clearance.

---

### 2. `sensor_mount_rcc.scad` — for RCC / concrete overhead tank

| PART value | What it is |
|---|---|
| `"plinth"` | Flat plinth + sensor tube, bolts to concrete slab |
| `"cap"` | Push-fit cap that locks US-100 in tube |

**How it mounts:** Drill 4× M6 holes (90×70mm pattern) and 1× 36mm centre hole in concrete. M6 anchor bolts hold plinth down. RTV sealant fills groove under plinth. Tube extends 40mm below slab into tank air space. US-100 slides into tube from top, cap locks it.

---

### 3. `electronics_box.scad` — weatherproof box (both tank types)

| PART value | What it is |
|---|---|
| `"body"` | Main box body |
| `"lid"` | Screw-on lid (4× M4) with gasket groove |
| `"pipe_clip_25"` | Pipe saddle for 25mm OD pipe |
| `"pipe_clip_32"` | Pipe saddle for 32mm OD pipe (standard ½" GI) |

**Holds:** 2× 32700 cells (horizontal), D1 Mini on shelf, MP1584 buck, CN3058E charger, 2S BMS.  
**Cable in:** PG7 gland on bottom → 4-core cable → sensor mount.  
**Charge:** USB-C panel-mount on right side.  
**Mount:** Two keyhole slots on back for wall screws, OR pipe clip bolts to same positions.

---

## Print settings (Bambu K1C)

| Setting | Value |
|---------|-------|
| Material | **ASA** — mandatory outdoors. PLA warps >60°C. |
| Layer height | 0.2mm |
| Wall loops | 4 |
| Infill | 40% gyroid |
| Supports | Auto — needed under sensor_mount_sintex arms and box lid gasket groove |
| Brim | 5mm (ASA warps without it) |
| Enclosure | Closed (K1C is enclosed ✅) |

---

## Key parameter to check before printing

**`sensor_mount_sintex.scad` line 1:** `BOSS_OD = 38.5`

Measure your Sintex tank lid's central hole with a calliper. Subtract 0.5mm. Set that value. If you print too big it won't fit; too small and it wobbles.

---

## One-time hardware needed

| Part | Where |
|------|-------|
| PG7 cable gland (× per sensor) | Any electronics shop |
| 2mm × 2mm EPDM square cord | Cut to length for gasket grooves |
| RTV silicone (clear) | Seal plinth base on RCC mount |
| M6 sleeve anchors × 4 | Only for RCC mount |
| M5 bolts × 3 + nuts | Sintex mount arm clamp |
| M3 set screws × 3 | Sintex clamp ring |
| IP67 USB-C panel-mount | Electronics box charge port |
