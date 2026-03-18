// ============================================================================
// sensor_mount_sintex.scad
//
// US-100 mount for Sintex overhead tank (black dome lid)
//
// US-100 REAL DIMENSIONS (verified from datasheet):
//   PCB:          44mm long × 20mm wide × 1.6mm thick
//   Two transducer cylinders on the FRONT face:
//     diameter:   16mm each
//     protrusion: ~12mm forward from PCB face
//     spacing:    ~26mm centre-to-centre (on the 44mm axis)
//   Total depth (back of PCB to tip of transducer): ~14mm
//   Pin header: 5 pins, 2.54mm pitch, on BACK of PCB at one short end
//   Mounting holes: 2mm dia, ~3mm from each short-end corner
//
// ORIENTATION IN THIS MOUNT:
//   PCB held HORIZONTALLY (flat), transducer face pointing STRAIGHT DOWN.
//   The two transducer cylinders hang through/over a slot in the mount floor.
//   Sound travels directly down into the tank water.
//
//                    SINTEX LID (top view)
//               ┌─────────────────────────┐
//               │    lid surface           │
//               │      ┌────────────┐      │
//               │      │  THIS PART │      │
//               │      │  sits on   │      │
//               │      │  the lid   │      │
//               │      └─────┬──────┘      │
//               └────────────┼─────────────┘
//                            │  boss through lid hole
//                            ▼ into tank
//
//                   CROSS-SECTION of mount:
//
//   ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ← lid surface
//       │                  │
//       │  [PCB flat]      │           ← PCB in tray
//       │ (T1)↓    (T2)↓   │           ← transducers pointing down
//       └──────────────────┘
//              │boss
//    ══════════╪═════════════           ← Sintex lid (8mm thick)
//              │
//              ↓ into tank air space
//
// TWO PARTS:
//   PART="body"   main mount that sits on lid + grips PCB in a tray
//   PART="clamp"  washer ring that goes below lid, tightens with M3 bolts
//
// KEY PARAMETER: BOSS_OD
//   Sintex tanks have a central vent hole. Typical ~38-42mm.
//   Measure yours, subtract 0.6mm → set BOSS_OD here.
//   If your lid has NO central hole: set USE_SCREW_FEET=true (4 screws into lid).
//
// PRINT: ASA, 4 walls, 0.2mm, 30% gyroid, brim 5mm
// ============================================================================

$fn = 80;

// ── US-100 actual dimensions ──────────────────────────────────────────────────
US_PCB_L     = 44.0;   // PCB length  (the long axis, 44mm)
US_PCB_W     = 20.0;   // PCB width   (the short axis, 20mm)
US_PCB_T     = 1.8;    // PCB thickness + 0.2 clearance
US_TRANS_D   = 16.5;   // transducer cylinder dia + 0.5 clearance
US_TRANS_H   = 13.0;   // transducer protrusion from PCB face (including clearance)
US_TRANS_PITCH = 26.0; // centre-to-centre spacing of the two transducers (along 44mm axis)
US_TRANS_OFFSET = 0.0; // offset of transducer pair from PCB centre (along 44mm axis)
US_PINS_W    = 5 * 2.54; // 5-pin header width = 12.7mm (on back face, one short end)
US_HOLE_D    = 2.2;    // mounting hole clearance (2mm holes)
// Mounting hole positions: 3mm from each short-end corner, 3mm from top/bottom edge
// i.e., at (±(US_PCB_L/2 - 3), ±(US_PCB_W/2 - 3)) — 4 holes total

// ── Mount body dimensions ─────────────────────────────────────────────────────
BODY_W   = US_PCB_L + 16;  // 60mm — PCB + 8mm wall each side
BODY_D   = US_PCB_W + 16;  // 36mm — PCB + 8mm wall each side
BODY_H   = 22;             // height above lid: PCB tray (18mm) + roof (4mm)
WT       = 3.5;            // wall thickness

// ── Lid-surface footprint ─────────────────────────────────────────────────────
// The mount sits flat on the Sintex lid surface.
// Three arms extend radially with M5 through-holes for self-tapping screws
// OR the boss clamp method (see CLAMP below).
ARM_COUNT   = 3;
ARM_W       = 14;
ARM_T       = 5;
ARM_L       = 30;          // arm length from body edge
ARM_SCREW_D = 5.3;         // M5 clearance

// ── Boss (drops into/through Sintex lid central hole) ────────────────────────
BOSS_OD    = 38.0;   // ← ADJUST to your lid hole dia - 0.6mm
BOSS_H     = 18;     // depth into tank (boss hangs below lid surface)
BOSS_WT    = 3.0;    // boss wall thickness
// Boss ID = BOSS_OD - 2*BOSS_WT = 32mm clearance for sound + air

// Boss floor is open — transducers hang through the slot into the boss tube.
// Sound beam travels through the open boss tube down into the tank.

// ── Clamp ring (below lid, locks mount from lifting off) ─────────────────────
CLAMP_OD   = BOSS_OD + 16;
CLAMP_T    = 5;

// ── Cable exit (PG7 gland, M12 thread, 12.2mm hole) ─────────────────────────
GLAND_D    = 12.2;
GLAND_BH   = 7;        // boss height for gland hole

// ── Helpers ───────────────────────────────────────────────────────────────────
module rounded_rect(w, d, h, r=4) {
    hull()
        for (x=[-w/2+r, w/2-r]) for (y=[-d/2+r, d/2+r-r*2])  // fix below
            translate([x, y, 0]) cylinder(h=h, r=r);
}

module arm_solid(angle, len=ARM_L) {
    rotate([0,0,angle])
    translate([BODY_W/2, 0, -ARM_T/2])
        hull() {
            cylinder(h=ARM_T, d=ARM_W, center=true);
            translate([len, 0, 0]) cylinder(h=ARM_T, d=ARM_W, center=true);
        }
}

// ─────────────────────────────────────────────────────────────────────────────
// PART: BODY — sits on Sintex lid, grips US-100 PCB face-down
// ─────────────────────────────────────────────────────────────────────────────
module mount_body() {

    // PCB tray floor Z (PCB sits here, face pointing down through slot)
    // Z=0 is the LID SURFACE (bottom of mount)
    // PCB top (back face) is at Z = WT + US_TRANS_H + US_PCB_T
    // PCB bottom (transducer face) is at Z = WT + US_TRANS_H
    // Transducer tips are at Z = WT (just above the slot/boss entry)
    TRAY_FLOOR_Z = WT + US_TRANS_H;  // floor that PCB back rests on

    difference() {
        union() {
            // Main body box (rounded rectangle)
            hull()
                for (x=[-(BODY_W/2-6), BODY_W/2-6])
                for (y=[-(BODY_D/2-6), BODY_D/2-6])
                    translate([x, y, 0]) cylinder(h=BODY_H, r=6);

            // Arms (3×, evenly spaced, at lid level Z=0 to Z=ARM_T)
            for (a=[0, 120, 240]) arm_solid(a);

            // Boss tube below lid surface (Z < 0)
            translate([0, 0, -BOSS_H])
                cylinder(h=BOSS_H+0.1, d=BOSS_OD);

            // Cable gland boss on side of body
            rotate([0,0,90])
            translate([BODY_D/2 - GLAND_BH/2, 0, BODY_H * 0.55])
                rotate([90,0,0])
                    cylinder(h=GLAND_BH, d=GLAND_D+8, center=true);
        }

        // ── Hollow out body interior ──────────────────────────────────────────
        translate([0, 0, WT])
            hull()
                for (x=[-(BODY_W/2-6-WT), BODY_W/2-6-WT])
                for (y=[-(BODY_D/2-6-WT), BODY_D/2-6-WT])
                    translate([x, y, 0]) cylinder(h=BODY_H, r=3);

        // ── Hollow out boss (open tube, sound + air path into tank) ──────────
        translate([0, 0, -BOSS_H-0.1])
            cylinder(h=BOSS_H+WT+0.2, d=BOSS_OD-2*BOSS_WT);

        // ── PCB tray slot in body floor ───────────────────────────────────────
        // Slot sized for PCB width (20mm) with 0.3mm each side = 20.6mm
        // Length = 44mm + 1mm = 45mm for easy insertion
        // Depth = PCB thickness = 1.8mm
        // The slot crosses the full body floor so PCB slides in from the side
        translate([0, 0, TRAY_FLOOR_Z])
            cube([US_PCB_L + 1.0, US_PCB_W + 0.6, US_PCB_T + 0.2], center=true);

        // ── Transducer relief (the two ⌀16mm cylinders hang below the PCB) ──
        // These holes in the floor let the transducers point through/past the floor
        for (sign=[-1,1])
            translate([sign * US_TRANS_PITCH/2, 0, 0])
                cylinder(h=TRAY_FLOOR_Z+0.2, d=US_TRANS_D);

        // ── Pin header clearance (5 pins on back of PCB, one short end) ──────
        // The header sticks up from the PCB back face by ~7mm
        translate([US_PCB_L/2 - US_PINS_W/2 - 2.54/2, 0,
                   TRAY_FLOOR_Z + US_PCB_T])
            cube([US_PINS_W + 2, US_PCB_W + 1, 10], center=true);

        // ── PCB retention clip recesses (snap lips on both long sides) ────────
        // Two small ledges that flex to let PCB in, then hold it
        for (sign=[-1,1])
            translate([0, sign*(US_PCB_W/2 + 0.3), TRAY_FLOOR_Z + US_PCB_T/2])
                rotate([0,90,0])
                    cylinder(h=US_PCB_L + 2, d=2, center=true);

        // ── Arm screw holes ───────────────────────────────────────────────────
        for (a=[0, 120, 240]) {
            arm_r = BODY_W/2 + ARM_L - 8;
            rotate([0,0,a])
            translate([arm_r, 0, -ARM_T/2-0.1])
                cylinder(h=ARM_T+0.2, d=ARM_SCREW_D, center=false);
        }

        // ── Cable gland hole ──────────────────────────────────────────────────
        rotate([0,0,90])
        translate([BODY_D/2 - GLAND_BH/2, 0, BODY_H * 0.55])
            rotate([90,0,0])
                cylinder(h=GLAND_BH+0.2, d=GLAND_D, center=true);
    }

    // ── PCB retention snap lips (thin ledges on long inner walls) ─────────────
    for (sign=[-1,1])
        translate([0, sign*(US_PCB_W/2 + WT - 0.5), TRAY_FLOOR_Z + US_PCB_T + 0.4])
            cube([US_PCB_L - 6, 1.2, 1.0], center=true);

    // ── PCB insertion guide chamfer label ────────────────────────────────────
    translate([0, -(BODY_D/2 - 2), BODY_H - 1])
        rotate([0,0,0])
            linear_extrude(1.2)
                text("← INSERT PCB", size=3.5,
                     font="Liberation Sans", halign="center", valign="center");
}

// ─────────────────────────────────────────────────────────────────────────────
// PART: CLAMP RING — slides up boss from below lid, locks with M3 set screws
// ─────────────────────────────────────────────────────────────────────────────
module clamp_ring() {
    difference() {
        cylinder(h=CLAMP_T, d=CLAMP_OD);

        // Boss bore
        translate([0,0,-0.1])
            cylinder(h=CLAMP_T+0.2, d=BOSS_OD+0.4);

        // 3× M3 radial set-screw holes
        for (a=[0,120,240])
            rotate([0,0,a])
            translate([BOSS_OD/2+4, 0, CLAMP_T/2])
                rotate([0,90,0])
                    cylinder(h=10, d=3.0, center=true);

        // Grip flats (finger tightening)
        for (a=[0,60,120,180,240,300])
            rotate([0,0,a])
            translate([CLAMP_OD/2-1, 0, CLAMP_T/2])
                cube([3, 4, CLAMP_T+0.2], center=true);
    }
}

// ── RENDER ────────────────────────────────────────────────────────────────────
// Change PART to export each piece:
PART = "body";
// PART = "clamp";
// PART = "both";    // exploded

if (PART == "body")       mount_body();
else if (PART == "clamp") clamp_ring();
else if (PART == "both") {
    mount_body();
    translate([0, 0, -(BOSS_H + CLAMP_T + 8)]) clamp_ring();
}
