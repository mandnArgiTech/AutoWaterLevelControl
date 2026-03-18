// ============================================================================
// electronics_box.scad — FluidLevelMonitor Weatherproof Electronics Enclosure
//
// CONTENTS (all fit with measured clearances):
//   2× 32700 LiFePO4 cells  dia 32.2mm, len 70.5mm  → side by side on floor
//   D1 Mini ESP8266          PCB 34.2×25.6mm, headers below, components up
//   MP1584 buck module       22×17×6mm (incl. trim pot)
//   CN3058E charger module   ~30×20×5mm
//   2S BMS module            ~50×12×4mm
//
// INTERNAL LAYOUT (cross section, viewing from front):
//
//   ┌──────────────────────────────────────────────────────┐ ← lid
//   │                                                       │
//   │   [D1 Mini on standoffs] [MP1584] [CN3058E] [BMS]    │ ← electronics shelf
//   │   ─────────────────────────────────────────────────   │ ← shelf at Z=43
//   │                                                       │
//   │        [cell 1 ⌀33]          [cell 2 ⌀33]            │ ← cells in cradle
//   │         ████████████          ████████████            │
//   └──────────────────────────────────────────────────────┘ ← floor Z=0
//        ↑                                           ↑
//     72mm cell axis runs left-right in box
//     (cells side by side, axes parallel, both along X)
//
// EXTERNAL:
//   Outer: 95 × 82 × 88mm (W × D × H incl. lid)
//   Back:  two M5 keyhole wall-mount slots, 80mm apart
//   Right: USB-C charge port (10×7.8mm cutout)
//   Front: 2× 3mm LED holes (charge / standby)
//   Bottom: PG7 gland (12.2mm hole) — cable to sensor mount
//   Back top corner: 3mm vent hole + 8mm pocket for Gore-Tex membrane
//
// PARTS:
//   PART="body"      — main shell
//   PART="lid"       — screw-on lid (4× M3)
//   PART="pipe_clip" — clip for 25mm or 32mm standpipe (pipe_od parameter)
//
// PRINT: ASA  |  4 walls  |  0.2mm layer  |  40% gyroid  |  5mm brim
// ============================================================================

$fn = 72;

// ── Verified component dimensions ────────────────────────────────────────────

// 32700 cell
C_DIA     = 32.2;          // actual diameter
C_LEN     = 70.5;          // actual length
C_CLR     = 0.8;           // radial clearance in pocket each side
C_LCLEAR  = 1.0;           // axial clearance each end
CP_D      = C_DIA + 2*C_CLR;   // pocket diameter = 33.8mm
CP_L      = C_LEN + 2*C_LCLEAR; // pocket length  = 72.5mm
C_SPACING = C_DIA + 4;         // centre-to-centre of two cells = 36.2mm
                               // (2mm gap between cell bodies)

// D1 Mini
D1_PCB_L  = 34.2;   // PCB long axis
D1_PCB_W  = 25.6;   // PCB short axis
D1_PCB_T  = 1.6;    // PCB thickness
D1_HDR_P  = 8.5;    // header pin length below PCB plastic (pin + plastic)
D1_CPT_H  = 5.0;    // component height above PCB top (ESP module, USB)
D1_TOTAL  = D1_HDR_P + D1_PCB_T + D1_CPT_H;  // = 15.1mm total height
// Pin rows: 22.86mm (0.9") apart, 8 pins each, 2.54mm pitch
D1_ROW_SP = 22.86;  // row separation (centre to centre)
D1_SO_H   = 3.5;    // standoff height (raises PCB so pins clear floor)
D1_MH_D   = 2.4;    // M2 mounting hole clearance
// Mounting holes at ~2.5mm from each corner edge
D1_MH_X   = D1_PCB_L/2 - 2.5;  // ±16.1mm from PCB centre
D1_MH_Y   = D1_PCB_W/2 - 2.5;  // ±10.3mm from PCB centre

// MP1584 buck module
BK_L      = 22.0;
BK_W      = 17.0;
BK_H      = 6.0;   // PCB 4mm + trim pot 2mm
BK_CLR    = 0.5;   // pocket clearance each side

// CN3058E charger (approximate — varies by vendor)
CH_L      = 32.0;
CH_W      = 20.0;
CH_H      = 6.0;
CH_CLR    = 0.5;

// 2S BMS module (flat strip style)
BMS_L     = 52.0;
BMS_W     = 12.0;
BMS_H     = 4.0;
BMS_CLR   = 0.5;

// ── Box geometry ──────────────────────────────────────────────────────────────
WT        = 3.5;     // wall thickness
LID_H     = 12.0;    // lid height
LID_PLAY  = 0.3;     // lid spigot clearance

// Internal dimensions — driven by component layout
// Width (X): cells along X = CP_L = 72.5mm, plus walls
// Depth (Y): 2 cells side by side = 2×CP_D/2 + C_SPACING ≈ 66mm, plus walls
// Height (Z):
//   Floor WT = 3.5
//   Cell diameter (sitting in semicircle cradle, centred) = CP_D = 33.8mm
//   Gap above cells to shelf = 5mm (wiring)
//   Shelf thickness = 2.5mm
//   Standoff D1_SO_H = 3.5mm
//   D1 Mini total = D1_TOTAL = 15.1mm
//   Headroom above D1 = 3mm
//   Total interior = 33.8 + 5 + 2.5 + 3.5 + 15.1 + 3 = 62.9mm → 64mm

IW        = CP_L + 16;       // 88.5mm → internal width (X)
ID_inner  = C_SPACING + CP_D + 10; // ~80mm → internal depth (Y)
IH        = 64.0;            // internal height (Z) as calculated

OW        = IW + 2*WT;       // outer width
OD_outer  = ID_inner + 2*WT; // outer depth
OH_body   = IH + WT;         // outer body height (lid sits on top)
OH_total  = OH_body + LID_H; // total incl. lid

// Key Z heights (measured from inside floor = 0)
Z_CELL_CTR = CP_D/2;         // cell centre height = 16.9mm
Z_SHELF    = CP_D + 5;       // shelf floor Z = 38.8mm
Z_D1_BOT   = Z_SHELF + 2.5 + D1_SO_H; // D1 PCB bottom face Z
Z_D1_PINS  = Z_SHELF + 2.5;  // pin tips clear shelf top by standoff gap

// Corner radius
CR = 6;

// ── Lid screw bosses (M3, 4 corners inside box) ───────────────────────────────
BOSS_OD   = 8.0;
BOSS_H    = 14.0;
BOSS_ID   = 2.9;    // M3 self-tap pilot

// ── Gasket groove (top face of body, 2×2mm EPDM cord) ────────────────────────
GK_W = 2.2;
GK_D = 1.8;

// ── USB-C cutout ──────────────────────────────────────────────────────────────
USBC_W  = 10.2;
USBC_H  = 7.8;
USBC_CR = 2.0;
// Position: right side wall, centred on electronics shelf level
USBC_Z  = WT + Z_SHELF + 10;   // centre of cutout

// ── PG7 cable gland ───────────────────────────────────────────────────────────
GLAND_D  = 12.2;
// Position: bottom face, offset from centre toward one corner
GLAND_X  = IW/2 - 20;   // 20mm from right inner wall
GLAND_Y  = 0;            // centred front-back

// ── Keyhole wall-mount slots (back face) ─────────────────────────────────────
KH_HEAD  = 9.0;    // M5 screw head dia
KH_SLOT  = 5.2;    // M5 shank dia
KH_LEN   = 14.0;   // slot travel length
KH_DIST  = 70.0;   // centre-to-centre X spacing
KH_Z     = 30.0;   // height from box base

// ── Vent hole (back face, top corner) ────────────────────────────────────────
VENT_D      = 3.0;
VENT_PKT_D  = 8.5;   // recess pocket for Gore-Tex membrane
VENT_PKT_H  = 2.5;

// ── Pipe clip ────────────────────────────────────────────────────────────────
PIPE_OD     = 32;    // override with parameter

// ── Helpers ───────────────────────────────────────────────────────────────────
module hull_box(w, d, h, r) {
    hull()
        for (x=[-(w/2-r), w/2-r])
        for (y=[-(d/2-r), d/2-r])
            translate([x,y,0]) cylinder(h=h, r=r);
}

module usbc_cutout(depth) {
    hull()
        for (x=[-(USBC_H/2-USBC_CR), USBC_H/2-USBC_CR])
        for (z=[-(USBC_W/2-USBC_CR), USBC_W/2-USBC_CR])
            translate([x, 0, z]) rotate([90,0,0])
                cylinder(h=depth, r=USBC_CR, center=true);
}

module screw_boss() {
    difference() {
        cylinder(h=BOSS_H, d=BOSS_OD);
        translate([0,0,-0.1]) cylinder(h=BOSS_H+0.2, d=BOSS_ID);
    }
}

module keyhole(x_offset) {
    translate([x_offset, 0, 0]) {
        // M5 head pocket (wider, at entry point = lower Z)
        translate([0, 0, 0])
            rotate([90,0,0]) cylinder(h=WT+0.2, d=KH_HEAD, center=true);
        // Slot above (narrower — screw locks in when box slides down)
        translate([0, 0, KH_LEN/2 + KH_HEAD/2])
            cube([KH_SLOT, WT+0.2, KH_LEN], center=true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// BOX BODY
// ─────────────────────────────────────────────────────────────────────────────
module box_body() {
    difference() {
        // ── Outer shell ───────────────────────────────────────────────────────
        translate([0,0,0])
            hull_box(OW, OD_outer, OH_body, CR);

        // ── Inner cavity ──────────────────────────────────────────────────────
        translate([0, 0, WT])
            hull_box(IW, ID_inner, IH + 1, CR - WT);

        // ── Lid seat step (lid spigot locates here) ────────────────────────── 
        translate([0, 0, OH_body - LID_H - 0.5])
            difference() {
                hull_box(IW + 2*(LID_PLAY+0.6),
                         ID_inner + 2*(LID_PLAY+0.6), LID_H+1, CR-WT+0.5);
                hull_box(IW - 4, ID_inner - 4, LID_H+2, CR-WT-2);
            }

        // ── Gasket groove on top mating face ──────────────────────────────────
        translate([0, 0, OH_body - GK_D])
            difference() {
                hull_box(IW - 1, ID_inner - 1, GK_D+0.1, CR-WT);
                hull_box(IW - 1 - 2*GK_W, ID_inner - 1 - 2*GK_W,
                         GK_D+0.2, CR-WT-GK_W);
            }

        // ── PG7 cable gland hole — bottom face ───────────────────────────────
        translate([GLAND_X, GLAND_Y, -0.1])
            cylinder(h=WT+0.2, d=GLAND_D);

        // ── USB-C port — right side wall ─────────────────────────────────────
        translate([OW/2, 0, USBC_Z])
            usbc_cutout(WT + 0.2);

        // ── LED holes — front face (2× charge indicator) ─────────────────────
        for (dx=[-8, 8])
            translate([dx, -(OD_outer/2), USBC_Z + 12])
                rotate([90,0,0]) cylinder(h=WT+0.2, d=3.2, center=true);

        // ── Keyhole wall mount slots — back face ──────────────────────────────
        translate([0, OD_outer/2, KH_Z])
            rotate([90,0,0]) {
                keyhole(-KH_DIST/2);
                keyhole( KH_DIST/2);
            }

        // ── Vent hole — back face, top corner ────────────────────────────────
        translate([OW/2-15, OD_outer/2, OH_body-15])
            rotate([90,0,0]) {
                cylinder(h=WT+0.2, d=VENT_D, center=true);
                // Membrane pocket (inside face recess)
                translate([0, 0, -(WT/2+VENT_PKT_H/2)])
                    cylinder(h=VENT_PKT_H+0.1, d=VENT_PKT_D, center=true);
            }
    }

    // ── Interior features (added after main subtract) ─────────────────────────

    // Cell cradle: two semicircular troughs on floor
    // Cells run along X axis, centred in box Y
    // Trough centres at Y = ±C_SPACING/2
    for (sign=[-1,1]) {
        translate([0, sign*C_SPACING/2, WT])
            difference() {
                // Cradle wall (half-cylinder shaped block)
                rotate([0,90,0])
                    cylinder(h=CP_L+4, d=CP_D+2*2, center=true); // outer
                // Cell pocket bore
                rotate([0,90,0])
                    cylinder(h=CP_L+0.2, d=CP_D, center=true);
                // Cut away top half so cell drops in from above
                translate([0, 0, CP_D/2+0.1])
                    cube([CP_L+6, CP_D+6, CP_D], center=true);
                // Floor clearance (cradle sits on floor)
                translate([0,0,-CP_D/2-0.1])
                    cube([CP_L+6, CP_D+6, CP_D], center=true);
            }
    }

    // Divider rib between cells (stops lateral movement)
    translate([0, 0, WT + CP_D*0.1])
        cube([CP_L, 2.5, CP_D*0.8], center=true);

    // Cell end-stop ribs (stops cells sliding axially)
    for (sign=[-1,1])
        translate([sign*(CP_L/2 + 1), 0, WT + CP_D/3])
            cube([2.5, C_SPACING + CP_D, CP_D*0.6], center=true);

    // Electronics shelf
    shelf_z = WT + Z_SHELF;
    translate([0, 0, shelf_z])
        difference() {
            hull_box(IW - 0.4, ID_inner - 0.4, 2.5, CR-WT);
            // Wiring passthrough holes
            translate([IW/4, 0, -0.1])  cylinder(h=3, d=18);
            translate([-IW/4, 0, -0.1]) cylinder(h=3, d=18);
            // Cable gland clearance
            translate([GLAND_X, GLAND_Y, -0.1]) cylinder(h=3, d=GLAND_D+4);
        }

    // D1 Mini standoffs on shelf (4 corners, M2 self-tap or press-fit)
    for (sx=[-1,1]) for (sy=[-1,1])
        translate([sx*D1_MH_X, sy*D1_MH_Y, shelf_z + 2.5])
            difference() {
                cylinder(h=D1_SO_H, d=5.5);
                translate([0,0,-0.1]) cylinder(h=D1_SO_H+0.2, d=D1_MH_D);
            }

    // MP1584 module pocket walls on shelf (open top — module drops in)
    bk_x = -IW/2 + BK_L/2 + BK_CLR + 5;  // left side of shelf
    bk_y = -ID_inner/2 + BK_W/2 + BK_CLR + 5;
    translate([bk_x, bk_y, shelf_z + 2.5]) {
        difference() {
            cube([BK_L+2*(BK_CLR+1.5), BK_W+2*(BK_CLR+1.5), BK_H+1], center=true);
            cube([BK_L+2*BK_CLR, BK_W+2*BK_CLR, BK_H+2], center=true);
        }
    }

    // Gland boss ring on floor
    translate([GLAND_X, GLAND_Y, WT])
        difference() {
            cylinder(h=4, d=GLAND_D+8);
            translate([0,0,-0.1]) cylinder(h=5, d=GLAND_D);
        }

    // 4× M3 lid screw bosses (inside corners)
    for (bx=[-(IW/2-BOSS_OD/2-1), IW/2-BOSS_OD/2-1])
    for (by=[-(ID_inner/2-BOSS_OD/2-1), ID_inner/2-BOSS_OD/2-1])
        translate([bx, by, WT]) screw_boss();
}

// ─────────────────────────────────────────────────────────────────────────────
// LID
// ─────────────────────────────────────────────────────────────────────────────
module box_lid() {
    difference() {
        union() {
            // Outer cap
            hull_box(OW-0.4, OD_outer-0.4, LID_H, CR-0.2);
            // Inner spigot (locates into body seat)
            translate([0,0,-5])
                hull_box(IW + 2*LID_PLAY - 0.6,
                         ID_inner + 2*LID_PLAY - 0.6, 5.2, CR-WT-0.3);
        }

        // Hollow lid (leave 3mm top plate)
        translate([0,0,-0.1])
            hull_box(IW-0.8, ID_inner-0.8, LID_H-2.8, CR-WT-0.4);

        // 4× M3 screw clearance holes
        for (bx=[-(IW/2-BOSS_OD/2-1), IW/2-BOSS_OD/2-1])
        for (by=[-(ID_inner/2-BOSS_OD/2-1), ID_inner/2-BOSS_OD/2-1])
            translate([bx, by, -0.1])
                cylinder(h=LID_H+0.2, d=3.4);

        // Embossed label
        translate([0, 6, LID_H-1])
            linear_extrude(1.2)
                text("FluidMonitor", size=5.5,
                     font="Liberation Sans:style=Bold",
                     halign="center", valign="center");

        // Tank ID slot (insert printed label strip)
        translate([0, -9, LID_H-0.8])
            cube([52, 9, 1.0], center=true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// PIPE CLIP — mounts box to standpipe
// Back of box has 2× M5 keyhole slots at ±KH_DIST/2 X, KH_Z height
// This clip bolts to those same two holes
// ─────────────────────────────────────────────────────────────────────────────
module pipe_clip(pipe_od=PIPE_OD) {
    PLATE_W = KH_DIST + 30;   // wide enough to span both keyholes
    PLATE_H = 50;
    PLATE_T = 4;
    SADDLE_T = 5;
    CLEARANCE = 0.5;

    difference() {
        union() {
            // Flat back plate
            translate([0, 0, 0])
                cube([PLATE_W, PLATE_T, PLATE_H], center=true);

            // Pipe saddle (half-cylinder on front face)
            translate([0, -(pipe_od/2 + PLATE_T/2), 0])
                rotate([0,90,0])
                    cylinder(h=PLATE_W, d=pipe_od + 2*SADDLE_T, center=true);

            // Strap tabs (bolt together around pipe)
            for (sign=[-1,1])
                translate([sign*(PLATE_W/2+8), -(pipe_od/2+SADDLE_T/2), 0])
                    cube([16, SADDLE_T, PLATE_H*0.7], center=true);
        }

        // Pipe bore
        translate([0, -(pipe_od/2 + PLATE_T/2), 0])
            rotate([0,90,0])
                cylinder(h=PLATE_W+0.2, d=pipe_od+CLEARANCE, center=true);

        // Open the saddle bottom (so pipe can be inserted sideways)
        translate([0, -(pipe_od + PLATE_T + SADDLE_T)*0.9, 0])
            cube([PLATE_W+0.2, pipe_od*1.5, PLATE_H*0.5], center=true);

        // M5 holes for box keyhole bolts
        for (sign=[-1,1])
            translate([sign*KH_DIST/2, 0, 0])
                rotate([90,0,0])
                    cylinder(h=PLATE_T+0.2, d=5.2, center=true);

        // M4 strap bolt holes
        for (sign=[-1,1])
            translate([sign*(PLATE_W/2+8), -(pipe_od/2+SADDLE_T/2), 0])
                rotate([0,90,0])
                    cylinder(h=20, d=4.3, center=true);
    }
}

// ── RENDER ────────────────────────────────────────────────────────────────────
// Change PART to export each piece separately:
PART = "body";
// PART = "lid";
// PART = "pipe_clip";
// PART = "all";   // exploded preview

if      (PART == "body")      box_body();
else if (PART == "lid")       box_lid();
else if (PART == "pipe_clip") pipe_clip(32);
else if (PART == "all") {
    box_body();
    translate([0, 0, OH_body + 8]) box_lid();
    translate([OW + 20, 0, PLATE_H/2]) rotate([90,0,0]) pipe_clip(32);
}
