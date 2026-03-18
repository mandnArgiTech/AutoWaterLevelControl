// ============================================================================
// FILE 2: sensor_mount_rcc.scad
//
// US-100 sensor mount for RCC / concrete overhead tank
//
// HOW IT WORKS:
//   A flat plinth bolts to the concrete top slab with 4× M6 anchor bolts.
//   A 35mm tube in the centre holds the US-100 pointing straight down.
//   The tube extends 40mm below the slab (into the tank air space).
//   A removable cap on top lets you insert/remove the US-100 PCB.
//   4-core cable exits the plinth via PG7 gland → down the wall → box.
//
//                 ┌─────── PLINTH ─────────┐
//                 │  ┌──────────────────┐  │
//     cap →  ──►  │  │   [US-100 PCB]   │  │ ← PG7 cable gland
//                 │  │    pointing down  │  │
//                 │  └──────────────────┘  │
//                 └───────────┬────────────┘
//        M6 bolt holes  ○     │     ○
//        (4× in corners)      │
//  ═══════════════════════════╪════════════  ← RCC slab top surface
//                             │ tube extends 40mm below slab
//                             ↓ into tank air space
//
// INSTALLATION:
//   1. Drill 4× 8mm holes in concrete (anchor bolt pattern 90×90mm)
//   2. Drill 1× 36mm hole in centre for tube (Forstner or hole saw)
//   3. Insert M6 sleeve anchors, bolt down plinth
//   4. Run sealant around plinth base
//   5. Drop US-100 into tube, connect cable, press cap on
//
// PRINT: ASA, 4 walls, 0.2mm
// ============================================================================

$fn = 72;

// ── Parameters ────────────────────────────────────────────────────────────────
// Plinth
PLINTH_W      = 120;    // plinth width (X)
PLINTH_D      = 90;     // plinth depth (Y)
PLINTH_H      = 12;     // plinth height above slab

// Bolt pattern (M6 anchor bolts, 90×90mm pattern measured centre-to-centre)
BOLT_PATTERN_X = 90;
BOLT_PATTERN_Y = 70;
BOLT_HOLE_D    = 6.5;   // M6 clearance
BOLT_CBORE_D   = 11;    // M6 hex head counterbore dia
BOLT_CBORE_H   = 6;     // counterbore depth

// Sensor tube
TUBE_OD        = 35;    // outer dia of sensor tube
TUBE_WT        = 2.5;   // tube wall thickness
TUBE_ID        = TUBE_OD - 2*TUBE_WT;  // = 30mm — fits US-100 (20.5mm wide)
TUBE_ABOVE     = 20;    // tube height above plinth top
TUBE_BELOW     = 40;    // tube extension below plinth (into tank air space)

// US-100 PCB pocket inside tube
US_W           = 21.0;  // PCB width + clearance (20.5 + 0.5)
US_L           = 45.0;  // PCB length + clearance (44.5 + 0.5)
US_H           = 17.0;  // PCB height + clearance (16 + 1)

// Cable gland boss (on plinth side face)
GLAND_HOLE     = 12.2;
GLAND_BOSS_T   = 8;

// Sealant groove around plinth base (holds RTV sealant)
SEAL_W         = 3;
SEAL_D         = 2;

WT             = 2.5;

// ── Plinth body ───────────────────────────────────────────────────────────────
module plinth() {
    difference() {
        union() {
            // Main plinth block with rounded corners
            hull() {
                for (x = [-(PLINTH_W/2 - 8), (PLINTH_W/2 - 8)])
                for (y = [-(PLINTH_D/2 - 8), (PLINTH_D/2 - 8)])
                    translate([x, y, 0])
                        cylinder(h=PLINTH_H, d=16, center=false);
            }

            // Sensor tube above plinth
            translate([0, 0, PLINTH_H])
                cylinder(h=TUBE_ABOVE, d=TUBE_OD, center=false);

            // Cable gland boss on side face (Y+ face)
            translate([0, PLINTH_D/2 - GLAND_BOSS_T/2, PLINTH_H * 0.6])
                rotate([90, 0, 0])
                    cylinder(h=GLAND_BOSS_T, d=GLAND_HOLE + 8, center=true);
        }

        // ── Sensor tube bore (hollow through plinth and tube above) ──────────
        translate([0, 0, -0.1])
            cylinder(h=PLINTH_H + TUBE_ABOVE + 0.2, d=TUBE_ID, center=false);

        // ── US-100 PCB pocket (oriented so PCB slides in from top) ───────────
        // PCB is wider than ID, so it slots into the tube on two guide rails
        // Orientation: PCB long axis along Y, transducers face DOWN
        translate([0, 0, PLINTH_H + TUBE_ABOVE - US_H - WT])
            cube([US_W, US_L, US_H + 0.1], center=true);

        // ── Bolt holes with counterbore ───────────────────────────────────────
        for (x = [-BOLT_PATTERN_X/2, BOLT_PATTERN_X/2])
        for (y = [-BOLT_PATTERN_Y/2, BOLT_PATTERN_Y/2])
            translate([x, y, 0]) {
                cylinder(h=PLINTH_H + 0.2, d=BOLT_HOLE_D, center=false);
                cylinder(h=BOLT_CBORE_H, d=BOLT_CBORE_D, center=false);
            }

        // ── Cable gland hole through side face ───────────────────────────────
        translate([0, PLINTH_D/2 - GLAND_BOSS_T/2, PLINTH_H * 0.6])
            rotate([90, 0, 0])
                cylinder(h=GLAND_BOSS_T + 0.2, d=GLAND_HOLE, center=true);

        // ── Tube extension below plinth (continues bore into tank) ────────────
        translate([0, 0, -TUBE_BELOW - 0.1])
            cylinder(h=TUBE_BELOW + 0.2, d=TUBE_ID, center=false);

        // ── Sealant groove on bottom face ─────────────────────────────────────
        translate([0, 0, -0.1])
            difference() {
                hull() {
                    for (x = [-(PLINTH_W/2 - 12), (PLINTH_W/2 - 12)])
                    for (y = [-(PLINTH_D/2 - 12), (PLINTH_D/2 - 12)])
                        translate([x, y, 0]) cylinder(h=SEAL_D+0.1, d=14, center=false);
                }
                hull() {
                    for (x = [-(PLINTH_W/2 - 12 - SEAL_W*2), (PLINTH_W/2 - 12 - SEAL_W*2)])
                    for (y = [-(PLINTH_D/2 - 12 - SEAL_W*2), (PLINTH_D/2 - 12 - SEAL_W*2)])
                        translate([x, y, -0.1]) cylinder(h=SEAL_D+0.3, d=14, center=false);
                }
            }
    }

    // Tube extension below plinth (outer wall only — bore already cut)
    translate([0, 0, -TUBE_BELOW])
        difference() {
            cylinder(h=TUBE_BELOW, d=TUBE_OD, center=false);
            translate([0,0,-0.1])
                cylinder(h=TUBE_BELOW+0.2, d=TUBE_ID, center=false);
        }

    // PCB retention snap ledge at bottom of pocket
    translate([0, 0, PLINTH_H + TUBE_ABOVE - US_H - WT - 0.3])
        difference() {
            cube([TUBE_ID - 0.2, TUBE_ID - 0.2, 0.6], center=true);
            cube([US_W - 3, US_L - 3, 0.8], center=true);
        }
}

// ── Tube cap (push-fit, locks US-100 in place) ───────────────────────────────
module tube_cap() {
    CAP_H = 12;
    SPIGOT_H = 8;
    SPIGOT_OD = TUBE_ID - 0.3;

    difference() {
        union() {
            // Outer flange
            cylinder(h=CAP_H - SPIGOT_H, d=TUBE_OD + 4, center=false);
            // Spigot into tube
            translate([0, 0, CAP_H - SPIGOT_H])
                cylinder(h=SPIGOT_H + 0.1, d=SPIGOT_OD, center=false);
        }
        // Through hole so cable can pass
        translate([0, 0, -0.1])
            cylinder(h=CAP_H + 0.2, d=8, center=false);

        // Grip knurling (cosmetic flats for finger grip)
        for (a = [0, 45, 90, 135, 180, 225, 270, 315])
            rotate([0, 0, a])
                translate([TUBE_OD/2 + 3, 0, (CAP_H - SPIGOT_H)/2])
                    cube([4, 3, CAP_H - SPIGOT_H + 0.2], center=true);
    }
}

// ── RENDER ────────────────────────────────────────────────────────────────────
PART = "plinth";
// PART = "cap";
// PART = "both";

if (PART == "plinth")     plinth();
else if (PART == "cap")   tube_cap();
else if (PART == "both") {
    plinth();
    translate([0, 0, PLINTH_H + TUBE_ABOVE + 5]) tube_cap();
}
