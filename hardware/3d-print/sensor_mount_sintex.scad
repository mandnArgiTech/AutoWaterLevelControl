// ============================================================================
// FILE 1: sensor_mount_sintex.scad
//
// US-100 sensor mount for Sintex overhead tank (black dome lid)
//
// HOW IT WORKS:
//   The Sintex dome lid has a central vent/fill hole (~38-42mm dia).
//   This mount has a boss that drops through that hole into the tank.
//   The US-100 PCB sits inside the boss tube, protected and pointing down.
//   Three radial arms rest on the lid surface — M5 screws clamp underneath.
//   A 4-core cable exits through a PG7 gland on the side of the hub.
//
//   Sintex lid hole dia: ~40mm (measure yours — use BOSS_OD parameter)
//   Sintex lid dome height: ~80mm above tank body
//
//                   ┌── HUB (holds US-100 inside) ──┐
//                   │                               │
//        ARM ───────┤       [US-100 PCB]            ├─────── ARM
//                   │       ↓ sound beam            │
//        ARM ───────┤                               │
//                   └────────────────────────────── ┘
//                               │ BOSS
//                        ═══════╪══════  ← Sintex lid surface
//                               │ (through lid hole)
//                               ↓ into tank  (sound travels down to water)
//
// 4-wire cable exits hub via PG7 gland → runs down pipe → electronics box
//
// PRINT: ASA, 3 walls, 0.2mm, 30% gyroid
// ============================================================================

$fn = 72;

// ── Key parameters — MEASURE YOUR TANK BEFORE PRINTING ───────────────────────
BOSS_OD       = 38.5;  // outer dia of boss (fits through Sintex lid hole)
                        // Measure your lid hole dia, subtract 0.5mm clearance
                        // Typical Sintex: 40mm hole → set 38.5 here
BOSS_H        = 35;    // boss depth below lid surface (into tank)
                        // Must be ≥ US100 height (16mm) + 15mm air gap = 31mm min

LID_THICK     = 8;     // Sintex lid thickness at the hole edge (measure)
CLAMP_GAP     = LID_THICK + 1;  // gap between hub base and clamp ring

// ── Hub (the main body above the lid) ────────────────────────────────────────
HUB_OD        = 60;    // outer dia of hub above lid
HUB_H         = 30;    // height of hub above lid surface
WT            = 3.0;   // wall thickness

// ── Arms ──────────────────────────────────────────────────────────────────────
ARM_COUNT     = 3;
ARM_W         = 14;    // arm width
ARM_T         = 5;     // arm thickness
ARM_L         = 45;    // arm length from hub edge to tip
ARM_SCREW_D   = 5.5;   // M5 hole in arm tip
ARM_R         = HUB_OD/2 + ARM_L - 10;  // screw hole radius from centre

// ── US-100 PCB pocket (inside hub, sensor faces DOWN into boss tube) ──────────
// US-100 PCB: 44.5 × 20.5 mm, height 16mm
// Sensor sits with PCB face DOWN — transducers point into boss tube
US_W          = 45.0;  // PCB width + 0.5 clearance
US_L          = 21.0;  // PCB length + 0.5 clearance
US_H          = 16.5;  // PCB height + 0.5 clearance
US_PCB_T      = 1.6;   // PCB board thickness (for retention ledge)

// ── Cable gland (PG7 thread = M12×1.5, hole = 12.0mm) ────────────────────────
GLAND_HOLE    = 12.2;
GLAND_BOSS_H  = 8;     // boss height around gland hole

// ── Clamp ring (below lid, holds mount from pulling up) ──────────────────────
CLAMP_OD      = BOSS_OD + 14;
CLAMP_T       = 4;

// ── Helpers ───────────────────────────────────────────────────────────────────
module arm(angle) {
    rotate([0, 0, angle])
    translate([HUB_OD/2, 0, -ARM_T/2]) {
        difference() {
            hull() {
                // Root at hub edge
                cylinder(h=ARM_T, d=ARM_W, center=true);
                // Tip
                translate([ARM_L, 0, 0])
                    cylinder(h=ARM_T, d=ARM_W, center=true);
            }
            // M5 screw hole near tip
            translate([ARM_L - 8, 0, 0])
                cylinder(h=ARM_T + 0.2, d=ARM_SCREW_D, center=true);
        }
    }
}

// ── MAIN BODY ─────────────────────────────────────────────────────────────────
module sensor_mount() {
    difference() {
        union() {
            // Hub cylinder above lid
            cylinder(h=HUB_H, d=HUB_OD);

            // Boss below hub that goes through Sintex lid hole
            translate([0, 0, -BOSS_H])
                cylinder(h=BOSS_H + 0.1, d=BOSS_OD);

            // Three arms radiating from hub base
            for (a = [0, 120, 240]) arm(a);

            // Cable gland boss on hub side
            rotate([0, 0, 60])
            translate([HUB_OD/2 - WT/2, 0, HUB_H/2])
                rotate([0, 90, 0])
                    cylinder(h=GLAND_BOSS_H, d=GLAND_HOLE + 6, center=true);
        }

        // ── Hollow out hub and boss (forms the sensor cavity + acoustic tube) ─
        translate([0, 0, -BOSS_H - 0.1])
            cylinder(h=HUB_H + BOSS_H + 0.2, d=BOSS_OD - 2*WT);

        // ── US-100 PCB pocket (open at top for insertion, closed at bottom) ───
        // Pocket is centred in hub, opens from the TOP
        translate([0, 0, HUB_H - US_H - WT])
            cube([US_W, US_L, US_H + 0.1], center=true);

        // ── Retention ledge slots for PCB edges (1.6mm × 1.5mm channels) ──────
        for (sign = [-1, 1])
            translate([sign * US_W/2, 0, HUB_H - US_H - WT - 0.5])
                rotate([90, 0, 0])
                    cube([US_PCB_T + 0.3, US_H * 0.6, US_L + 0.2], center=true);

        // ── Cable routing channel from pocket to gland (through hub wall) ──────
        rotate([0, 0, 60])
        translate([HUB_OD/2 - WT/2, 0, HUB_H/2])
            rotate([0, 90, 0])
                cylinder(h=GLAND_BOSS_H + 0.2, d=GLAND_HOLE, center=true);

        // ── Open bottom of boss (acoustic path — sound travels here) ──────────
        // Boss is already hollow from the main cylinder subtraction above
        // Nothing extra needed — the tube IS the acoustic path

        // ── Arm screw holes (already in arm module, but clear through hub base)─
        for (a = [0, 120, 240])
            rotate([0, 0, a])
                translate([ARM_R, 0, -ARM_T/2 - 0.1])
                    cylinder(h=ARM_T + 0.2, d=ARM_SCREW_D, center=false);
    }

    // ── Snap ledge at bottom of PCB pocket (stops PCB falling through) ────────
    translate([0, 0, HUB_H - US_H - WT - 0.4])
        difference() {
            cube([US_W + 2, US_L + 2, 0.8], center=true);
            cube([US_W - 4, US_L - 4, 1.0], center=true);
        }
}

// ── CLAMP RING (printed separately, slips over boss, tightened with screws) ───
module clamp_ring() {
    // Slides up boss from below the lid, M3 set screws lock it in place
    difference() {
        cylinder(h=CLAMP_T, d=CLAMP_OD, center=false);

        // Boss hole
        translate([0, 0, -0.1])
            cylinder(h=CLAMP_T + 0.2, d=BOSS_OD + 0.3, center=false);

        // 3× M3 set screw holes radially
        for (a = [0, 120, 240])
            rotate([0, 0, a])
                translate([BOSS_OD/2 + 4, 0, CLAMP_T/2])
                    rotate([0, 90, 0])
                        cylinder(h=8, d=2.9, center=true);
    }
}

// ── RENDER ────────────────────────────────────────────────────────────────────
PART = "mount";
// PART = "clamp";
// PART = "both";      // exploded view for checking

if (PART == "mount")       sensor_mount();
else if (PART == "clamp")  clamp_ring();
else if (PART == "both") {
    sensor_mount();
    translate([0, 0, -(BOSS_H + CLAMP_T + 5)]) clamp_ring();
}
