// ============================================================================
// FILE 3: electronics_box.scad
//
// Weatherproof electronics box — D1 Mini + 2× 32700 + power modules
// Mounts on wall (2 screws) or on pipe (included pipe clip)
//
// CONTENTS:
//   • D1 Mini ESP8266 (on PCB tray, socketed)
//   • 2× 32700 LiFePO4 cells (vertical, side by side)
//   • MP1584 buck module (17×11mm)
//   • CN3058E charger module (30×18mm typical)
//   • 2S BMS module (40×10mm typical)
//   • 100kΩ + 24kΩ battery divider resistors
//
// CABLE ENTRY: PG7 gland on bottom → 4-core cable → sensor
// CHARGE PORT: USB-C panel-mount on right side
// MOUNT: Two keyhole slots on back for wall screws
//         OR pipe clip prints separately and bolts to back
//
// INTERNAL LAYOUT (side view, lid on left):
//
//   LID │ [USB-C]    [D1 Mini on tray]  [CN3058E] │ BACK
//       │                                 [MP1584]  │
//       │   [Cell 1]    [Cell 2]          [BMS]    │
//       │                                           │
//       │              [PG7 gland cable exit]       │
//
// PRINT: ASA, 4 walls, 0.2mm layers
// ============================================================================

$fn = 72;

// ── Box outer dimensions ──────────────────────────────────────────────────────
BW = 130;    // width  (X) — enough for 2× cells (66mm) + electronics
BD = 75;     // depth  (Y) — front to back
BH = 70;     // height (Z) — cell height (72mm) + floor + lid clearance... 
             //              cells are horizontal so fits in 70mm Y
WT = 3.5;    // wall thickness

// Inner cavity
IW = BW - 2*WT;  // 123
ID = BD - 2*WT;  // 68
IH = BH - 2*WT;  // 63

// ── Lid ───────────────────────────────────────────────────────────────────────
LID_H     = 12;   // lid height
LID_PLAY  = 0.3;  // clearance

// ── Corner radius ─────────────────────────────────────────────────────────────
CR = 7;

// ── M4 lid screws (4 corner bosses) ──────────────────────────────────────────
BOSS_OD = 9;
BOSS_H  = 14;
BOSS_ID = 3.3;    // M4 self-tap

// ── 32700 cells — horizontal orientation ─────────────────────────────────────
// Cells lie on their side along the box width
// Diameter 32mm, length 70mm, clearance +1mm each
CELL_D    = 33;
CELL_L    = 71;
// Two cells side by side along Y axis:
//   Total Y span = 2 × 33 = 66mm < ID(68) ✓
//   Cell length (71mm) along X axis < IW(123) ✓
// Cells rest on floor, against back wall

// ── PCB tray rail positions ───────────────────────────────────────────────────
TRAY_Z    = WT + CELL_D + 5;   // shelf above cells

// ── Cable gland (PG7, hole 12mm) on bottom face ──────────────────────────────
GLAND_D   = 12.2;
GLAND_X   = -IW/2 + 20;   // offset from centre toward front-left
GLAND_BOSS_H = 5;

// ── USB-C cutout on right side wall ──────────────────────────────────────────
USBC_W    = 10.2;
USBC_H    = 7.8;
USBC_CR   = 2.0;
USBC_Z    = WT + CELL_D + 8;   // above cells, aligned with electronics shelf

// ── LED holes (2× 3mm, right side wall, above USB-C) ────────────────────────
LED_D     = 3.2;
LED_Z     = USBC_Z + 14;

// ── Gasket groove (top of body, for 2×2mm EPDM cord) ─────────────────────────
GK_W = 2.2;
GK_D = 1.8;

// ── Wall mount keyhole slots (back face) ─────────────────────────────────────
KH_D  = 8;       // keyhole head diameter (fits M5 screw head)
KH_SD = 4.5;     // keyhole slot diameter (M5 shank passes through)
KH_SL = 12;      // slot length
KH_SPACING = 90; // centre-to-centre X

// ── Helpers ───────────────────────────────────────────────────────────────────
module rounded_rect(w, d, h, r) {
    hull() {
        for (x = [-(w/2-r), (w/2-r)])
        for (y = [-(d/2-r), (d/2-r)])
            translate([x, y, 0]) cylinder(h=h, r=r, center=false);
    }
}

module usbc_hole(depth) {
    hull() {
        for (x = [-(USBC_H/2-USBC_CR), (USBC_H/2-USBC_CR)])
        for (z = [-(USBC_W/2-USBC_CR), (USBC_W/2-USBC_CR)])
            translate([x, 0, z]) rotate([90,0,0])
                cylinder(h=depth, d=USBC_CR*2, center=true);
    }
}

module screw_boss() {
    difference() {
        cylinder(h=BOSS_H, d=BOSS_OD);
        translate([0, 0, -0.1]) cylinder(h=BOSS_H+0.2, d=BOSS_ID);
    }
}

module keyhole_slot(x_pos) {
    // Vertical keyhole: round head at bottom, slot upward
    translate([x_pos, 0, 0]) {
        // Slot (M5 shank — 5mm wide)
        translate([0, 0, KH_SL/2 + KH_D/2])
            cube([KH_SD, WT+0.2, KH_SL], center=true);
        // Head (M5 head — 9mm dia)
        translate([0, 0, KH_D/2])
            rotate([90,0,0])
                cylinder(h=WT+0.2, d=KH_D, center=true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// BOX BODY
// ─────────────────────────────────────────────────────────────────────────────
module box_body() {
    body_h = BH - LID_H;

    difference() {
        // Outer shell
        rounded_rect(BW, BD, body_h, CR);

        // Inner cavity
        translate([0, 0, WT])
            rounded_rect(IW, ID, body_h, CR - WT);

        // Lid seat (shallow step at top for lid to locate)
        translate([0, 0, body_h - LID_H - 0.5])
            difference() {
                rounded_rect(IW + 2*LID_PLAY + 1, ID + 2*LID_PLAY + 1,
                             LID_H + 1, CR - WT);
                rounded_rect(IW - 4, ID - 4, LID_H + 2, CR - WT - 2);
            }

        // Gasket groove in top face
        translate([0, 0, body_h - GK_D])
            difference() {
                rounded_rect(IW - 2, ID - 2, GK_D + 0.1, CR - WT - 1);
                rounded_rect(IW - 2 - 2*GK_W, ID - 2 - 2*GK_W,
                             GK_D + 0.2, CR - WT - 1 - GK_W);
            }

        // Cable gland hole — bottom face
        translate([GLAND_X, 0, -0.1])
            cylinder(h=WT + 0.2, d=GLAND_D);

        // USB-C cutout — right side wall
        translate([BW/2, 0, USBC_Z])
            rotate([0, 0, 0])
                usbc_hole(WT + 0.2);

        // LED holes — right side wall
        for (dz = [0, 8])
            translate([BW/2 + 0.1, 10, LED_Z + dz])
                rotate([0, 90, 0])
                    cylinder(h=WT + 0.2, d=LED_D, center=true);

        // Keyhole slots — back face
        translate([0, BD/2, 20])
            rotate([90, 0, 0]) {
                keyhole_slot(-KH_SPACING/2);
                keyhole_slot( KH_SPACING/2);
            }
    }

    // ── Interior features ─────────────────────────────────────────────────────

    // Cell cradle ribs (3 thin ribs between and around cells)
    // Cells run along X axis, stack in Y
    translate([0, 0, WT]) {
        // Rib between the two cells
        cube([CELL_L + 2, 2, CELL_D * 0.5], center=true);
        // Back stop (stops cells sliding backward)
        translate([0, CELL_D + 2, CELL_D/4])
            cube([CELL_L + 2, 2, CELL_D/2 + 2], center=true);
    }

    // Electronics shelf (above cells)
    translate([0, 0, TRAY_Z]) {
        difference() {
            rounded_rect(IW - 0.4, ID - 0.4, 2.5, CR - WT);
            // Wire passthrough
            translate([IW/4, 0, -0.1]) cylinder(h=3, d=18);
            // Cable from gland
            translate([GLAND_X, 0, -0.1]) cylinder(h=3, d=16);
        }
    }

    // 4 corner screw bosses for lid
    for (x = [-(IW/2 - BOSS_OD/2 - 1), (IW/2 - BOSS_OD/2 - 1)])
    for (y = [-(ID/2 - BOSS_OD/2 - 1), (ID/2 - BOSS_OD/2 - 1)])
        translate([x, y, WT])
            screw_boss();

    // Gland boss ring on floor
    translate([GLAND_X, 0, WT + 0.1])
        difference() {
            cylinder(h=GLAND_BOSS_H, d=GLAND_D + 8);
            translate([0,0,-0.1]) cylinder(h=GLAND_BOSS_H+0.2, d=GLAND_D);
        }
}

// ─────────────────────────────────────────────────────────────────────────────
// LID
// ─────────────────────────────────────────────────────────────────────────────
module box_lid() {
    difference() {
        union() {
            // Outer cap
            rounded_rect(BW - 0.4, BD - 0.4, LID_H, CR - 0.2);
            // Inner spigot
            translate([0, 0, -5])
                rounded_rect(IW + 2*LID_PLAY - 0.8,
                             ID + 2*LID_PLAY - 0.8, 5.2, CR - WT - 0.3);
        }

        // Hollow lid (leave 3mm top plate)
        translate([0, 0, -0.1])
            rounded_rect(IW - 1, ID - 1, LID_H - 2.9, CR - WT - 0.5);

        // 4× M4 screw holes (matching bosses)
        for (x = [-(IW/2 - BOSS_OD/2 - 1), (IW/2 - BOSS_OD/2 - 1)])
        for (y = [-(ID/2 - BOSS_OD/2 - 1), (ID/2 - BOSS_OD/2 - 1)])
            translate([x, y, -0.1])
                cylinder(h=LID_H + 0.2, d=3.4);

        // Label recess + tank ID
        translate([0, 8, LID_H - 1])
            linear_extrude(1.3)
                text("FluidMonitor", size=6,
                     font="Liberation Sans:style=Bold",
                     halign="center", valign="center");

        // Blank slot for printed label strip (tank name)
        translate([0, -10, LID_H - 0.8])
            cube([55, 10, 1], center=true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// PIPE CLIP (mounts box to 25mm or 32mm pipe standpipe beside tank)
// Bolts to the two keyhole screw positions on box back
// ─────────────────────────────────────────────────────────────────────────────
module pipe_clip(pipe_od=32) {
    // Pipe clip that wraps around pipe and has two M5 threaded inserts
    // to bolt to back of box
    CLIP_W  = 40;
    CLIP_H  = 30;
    CLIP_T  = 5;
    PIPE_CLEARANCE = 0.5;

    difference() {
        union() {
            // Back plate
            cube([CLIP_W, CLIP_T, CLIP_H], center=true);
            // Pipe saddle (semicircle)
            translate([0, -(pipe_od/2 + CLIP_T/2), 0])
                rotate([0, 90, 0])
                    cylinder(h=CLIP_W, d=pipe_od + 8, center=true);
        }

        // Pipe bore
        translate([0, -(pipe_od/2 + CLIP_T/2), 0])
            rotate([0, 90, 0])
                cylinder(h=CLIP_W + 0.2, d=pipe_od + PIPE_CLEARANCE, center=true);

        // M5 holes for box bolts (KH_SPACING apart)
        for (x = [-KH_SPACING/2, KH_SPACING/2])
            translate([x, 0, 0])
                rotate([90, 0, 0])
                    cylinder(h=CLIP_T + 0.2, d=5.2, center=true);

        // Cut to open the saddle (so it can be clamped around pipe)
        translate([0, -(pipe_od + CLIP_T + 8)/2, 0])
            cube([CLIP_W + 0.2, pipe_od + 8, 3], center=true);
    }

    // Clip strap tabs with M4 bolt holes (two halves bolt together around pipe)
    for (sign = [-1, 1])
        translate([sign * (CLIP_W/2 + 8), -(pipe_od/2 + CLIP_T), 0])
            difference() {
                cube([16, 8, CLIP_H], center=true);
                rotate([0, 90, 0])
                    cylinder(h=20, d=4.3, center=true);
            }
}

// ── RENDER ────────────────────────────────────────────────────────────────────
PART = "body";
// PART = "lid";
// PART = "pipe_clip_25";   // for 25mm OD pipe
// PART = "pipe_clip_32";   // for 32mm OD pipe
// PART = "all";            // exploded assembly view

if (PART == "body")            box_body();
else if (PART == "lid")        box_lid();
else if (PART == "pipe_clip_25") pipe_clip(25);
else if (PART == "pipe_clip_32") pipe_clip(32);
else if (PART == "all") {
    box_body();
    translate([0, 0, BH - LID_H + 5]) box_lid();
    translate([BW + 20, 0, 0]) pipe_clip(32);
}
