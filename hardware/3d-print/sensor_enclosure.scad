// ============================================================================
// FluidLevelMonitor — Sensor Node Enclosure
// D1 Mini + US-100 + 2× 32700 LiFePO4 cells
//
// Printer: Bambu Lab K1C (or any FDM printer, 0.4mm nozzle)
// Material: ASA or PETG (UV/heat resistant for outdoor use)
//           DO NOT use PLA — melts in Indian summer sun (>60°C in enclosure)
//
// Prints:
//   1. enclosure_body   — main box with battery bays, PCB rail, cable gland hole
//   2. enclosure_lid    — snap+screw lid, USB-C cutout, gasket groove
//   3. sensor_bracket   — mounts US-100 over tank opening, cable tie slots
//   4. pcb_tray         — snap-in tray holding D1 Mini, buck, charger modules
//   5. battery_cradle   — cradle holding 2× 32700 cells side by side
//
// Assembly:
//   Body + Lid sealed with 2mm EPDM o-ring in groove → ~IP65
//   US-100 cable exits through 12mm hole → PG7 cable gland → IP67
//   Charge port: 12mm panel-mount USB-C cutout with rubber cap
//
// Print settings (ASA):
//   Layer height: 0.2mm
//   Walls: 4 perimeters (≥3.2mm — needed for weatherproofing)
//   Infill: 40% gyroid
//   Supports: Enabled for lid gasket grove (overhang ~60°)
//   Brim: 5mm (ASA warps — use enclosure adhesive or draft shield)
// ============================================================================

// ── Global parameters (all in mm) ────────────────────────────────────────────
$fn = 64;         // circle resolution — keep high for holes

// Enclosure outer dimensions
EW = 122;         // external width  (X)
EH = 96;          // external height (Y)
ED = 60;          // external depth  (Z)
WT = 3.5;         // wall thickness (4 perimeters × 0.86mm ≈ 3.44 — use 3.5)
LT = 3.5;         // lid thickness

// Internal cavity
IW = EW - 2*WT;   // = 115
IH = EH - 2*WT;   // = 89
ID = ED - WT - LT; // = 53

// Corner radius
CR = 6;

// Lid
LID_H    = LT + 4;   // total lid height (includes gasket rim)
LID_PLAY = 0.3;      // fit clearance lid→body

// Screw boss (M3 self-tap)
BOSS_OD = 8;
BOSS_H  = 12;
BOSS_ID = 2.8;    // M3 self-tap pilot hole

// ── Component dimensions ──────────────────────────────────────────────────────
// 32700 cell
CELL_D  = 32.5;   // diameter (actual 32.0, +0.5 clearance)
CELL_L  = 71.0;   // length   (actual 70.0, +1.0 clearance)

// D1 Mini PCB
D1_W    = 26;     // PCB width
D1_L    = 34.5;   // PCB length (without pin headers)
D1_H    = 11;     // height with components on top

// PCB tray (holds D1 Mini + buck + charger)
TRAY_W  = IW - 4;  // leaves 2mm each side
TRAY_L  = 58;
TRAY_H  = 15;

// US-100 sensor
US_W    = 44.5;
US_D    = 20.5;
US_H    = 16;
US_HOLE = 12.2;   // cable gland thread OD (PG7 inner = 12.4)

// USB-C panel mount cutout
USBC_W  = 10.0;
USBC_H  = 7.5;
USBC_CR = 2.0;

// Ventilation / pressure equalization (Gore-Tex membrane goes here if desired)
// Covered with adhesive membrane — 3mm hole, recessed 8mm pocket
VENT_D  = 3.0;
VENT_POCKET = 8.0;

// LED indicator holes (charge / standby)
LED_D   = 3.2;

// ── Derived positions ─────────────────────────────────────────────────────────
// Battery bay: cells sit side by side, centered in lower half of box
BATT_Y_OFFSET = IH/2 - CELL_D - 4;  // bottom half, 4mm from wall
BATT_X_CENTER = 0;

// PCB tray sits above battery bay
TRAY_Y_OFFSET = -IH/2 + CELL_D + 8 + TRAY_H/2;

// USB-C on right side wall, 20mm from bottom
USBC_Z = -ED/2 + WT + 20;

// Cable gland (US-100) on bottom wall, centered X, toward front
GLAND_X = 0;
GLAND_Y = -IH/2 + 20;   // 20mm from front

// ── Utilities ─────────────────────────────────────────────────────────────────
module rounded_box(w, h, d, r, center=true) {
    // Rounded-corner box using hull of 4 cylinders
    translate(center ? [0,0,0] : [w/2, h/2, d/2])
    hull() {
        for (x = [-(w/2-r), (w/2-r)])
        for (y = [-(h/2-r), (h/2-r)])
            translate([x, y, 0])
                cylinder(h=d, r=r, center=true);
    }
}

module rounded_box_2d(w, h, r) {
    hull() {
        for (x = [-(w/2-r), (w/2-r)])
        for (y = [-(h/2-r), (h/2-r)])
            translate([x, y]) circle(r=r);
    }
}

module screw_boss(h=BOSS_H, od=BOSS_OD, id=BOSS_ID) {
    difference() {
        cylinder(h=h, d=od, center=false);
        translate([0, 0, -0.1])
            cylinder(h=h+0.2, d=id, center=false);
    }
}

// ── PART 1: ENCLOSURE BODY ────────────────────────────────────────────────────
module enclosure_body() {
    difference() {
        // Outer shell
        rounded_box(EW, EH, ED, CR);

        // Inner cavity (subtract lid thickness from top)
        translate([0, 0, LT/2])
            rounded_box(IW, IH, ID + LT + 0.1, CR - WT);

        // Lid slot — step for lid to sit in (1.5mm deep, 1.5mm wide)
        translate([0, 0, ED/2 - LT - 1.5])
            difference() {
                rounded_box(IW + 3, IH + 3, LT + 2, CR - WT + 1.5);
                rounded_box(IW - 3, IH - 3, LT + 3, CR - WT - 1.5);
            }

        // Gasket groove in lid mating face (1.5mm wide × 2mm deep, for 2mm EPDM o-ring)
        translate([0, 0, ED/2 - LT - 0.5])
            difference() {
                rounded_box(IW - 2, IH - 2, 2.2, CR - WT - 1);
                rounded_box(IW - 5, IH - 5, 2.3, CR - WT - 2.5);
            }

        // Cable gland hole (PG7, 12mm thread) — bottom face
        translate([GLAND_X, GLAND_Y, -ED/2 - 0.1])
            cylinder(h=WT + 0.2, d=US_HOLE);

        // USB-C panel mount — right side wall
        translate([EW/2 - WT/2, 0, USBC_Z])
            rotate([0, 90, 0])
                hull() {
                    for (dy = [-(USBC_H/2 - USBC_CR), (USBC_H/2 - USBC_CR)])
                    for (dz = [-(USBC_W/2 - USBC_CR), (USBC_W/2 - USBC_CR)])
                        translate([dy, dz, 0])
                            cylinder(h=WT + 0.2, r=USBC_CR, center=true);
                }

        // LED holes (2× 3mm) — front face, near USB-C side
        for (dz = [10, 16]) {
            translate([EW/2 - WT/2 - 6, EH/2 - WT/2, USBC_Z + dz])
                rotate([90, 0, 0])
                    cylinder(h=WT + 0.2, d=LED_D, center=true);
        }

        // Pressure equalization vent — back face, top corner
        translate([-EW/2 + WT/2, EH/2 - WT/2, ED/2 - 20])
            rotate([90, 0, 0]) {
                cylinder(h=WT + 0.2, d=VENT_D, center=true);
                // Recessed pocket for Gore-Tex membrane
                translate([0, 0, WT/2 - 2])
                    cylinder(h=4, d=VENT_POCKET, center=true);
            }
    }

    // ── Interior features ────────────────────────────────────────────────────

    // Battery bay walls — two half-cylinders as cell cradle ribs
    for (sign = [-1, 1]) {
        translate([sign * (CELL_D + 1), BATT_Y_OFFSET, -ED/2 + WT])
            rotate([0, 0, 0]) {
                // vertical rib between cells
                cube([2, CELL_D + 4, CELL_L * 0.6], center=true);
            }
    }

    // PCB tray guide rails (2mm × 5mm, on both long walls)
    for (sign = [-1, 1]) {
        translate([sign * (IW/2 - 1), TRAY_Y_OFFSET, -ED/2 + WT + CELL_D + 6])
            cube([2, TRAY_L, 3], center=true);
    }

    // Screw bosses — 4 corners of lid, 3mm from each corner wall
    for (x = [-(IW/2 - BOSS_OD/2 - 2), (IW/2 - BOSS_OD/2 - 2)])
    for (y = [-(IH/2 - BOSS_OD/2 - 2), (IH/2 - BOSS_OD/2 - 2)])
        translate([x, y, -ED/2 + WT])
            screw_boss(h=BOSS_H);

    // Cable gland boss (raised ring around hole for gland thread grip)
    translate([GLAND_X, GLAND_Y, -ED/2 + WT])
        difference() {
            cylinder(h=4, d=US_HOLE + 6, center=false);
            translate([0,0,-0.1]) cylinder(h=5, d=US_HOLE, center=false);
        }
}

// ── PART 2: LID ───────────────────────────────────────────────────────────────
module enclosure_lid() {
    difference() {
        union() {
            // Outer lid plate
            rounded_box(EW - 0.6, EH - 0.6, LID_H, CR - 0.3);

            // Inner rim that fits into body step (3mm tall, snug fit)
            translate([0, 0, -LID_H/2 + 1.5])
                rounded_box(IW - LID_PLAY*2 - 0.6, IH - LID_PLAY*2 - 0.6, 3, CR - WT - 0.3);
        }

        // M3 screw holes (countersunk 6mm × 1.5mm)
        for (x = [-(IW/2 - BOSS_OD/2 - 2), (IW/2 - BOSS_OD/2 - 2)])
        for (y = [-(IH/2 - BOSS_OD/2 - 2), (IH/2 - BOSS_OD/2 - 2)])
            translate([x, y, 0]) {
                cylinder(h=LID_H + 0.2, d=3.4, center=true);
                // countersink
                translate([0, 0, LID_H/2 - 1.4])
                    cylinder(h=1.6, d1=3.4, d2=6.5, center=false);
            }

        // Label recess (embossed text area) — 2mm deep
        translate([0, 10, LID_H/2 - 1])
            rounded_box(70, 20, 2.2, 3);
    }

    // Embossed label text
    translate([0, 10, LID_H/2 - 1.2])
        linear_extrude(1.4)
            text("FluidMonitor", size=6, font="Liberation Sans:style=Bold",
                 halign="center", valign="center");
}

// ── PART 3: PCB TRAY ─────────────────────────────────────────────────────────
module pcb_tray() {
    // Snap-in tray: holds D1 Mini (socketed on headers), buck module, charger
    RAIL_W = 2;
    FLOOR_T = 2;

    difference() {
        union() {
            // Tray floor
            cube([TRAY_W, TRAY_L, FLOOR_T], center=true);

            // Side walls
            for (sign = [-1, 1])
                translate([sign * (TRAY_W/2 - RAIL_W/2), 0, TRAY_H/2 - FLOOR_T/2])
                    cube([RAIL_W, TRAY_L, TRAY_H], center=true);

            // Snap tabs on side walls (clip into body rails)
            for (sign = [-1, 1])
                translate([sign * (TRAY_W/2 + 0.8), 0, FLOOR_T/2 + 1])
                    cube([1.6, 20, 3], center=true);
        }

        // D1 Mini mounting holes (2.5mm, 23×16mm pitch)
        for (dx = [-11.5, 11.5])
        for (dy = [-8, 8])
            translate([dx, dy - 8, 0])
                cylinder(h=FLOOR_T + 0.2, d=2.6, center=true);

        // Wiring passthrough slot (12×8mm, center of tray floor)
        translate([0, 5, 0])
            cube([12, 8, FLOOR_T + 0.2], center=true);
    }

    // D1 Mini standoffs (2.5mm holes, 3mm tall)
    for (dx = [-11.5, 11.5])
    for (dy = [-8, 8])
        translate([dx, dy - 8, FLOOR_T/2 + 1.5])
            difference() {
                cylinder(h=3, d=5, center=true);
                cylinder(h=3.2, d=2.6, center=true);
            }
}

// ── PART 4: BATTERY CRADLE ───────────────────────────────────────────────────
module battery_cradle() {
    // Holds 2× 32700 cells (⌀32.5, 71mm long) side by side
    CRADLE_W = CELL_D * 2 + 6;    // 71mm
    CRADLE_L = CELL_L + 4;        // 75mm
    FLOOR    = 2;
    WALL     = 2.5;

    difference() {
        union() {
            // Outer walls
            rounded_box(CRADLE_W, CRADLE_L, CELL_D/2 + FLOOR + 4, 3);
        }

        // Cell pockets (semi-circular in cross-section)
        for (sign = [-1, 1]) {
            translate([sign * (CELL_D/2 + 1), 0, FLOOR + 1])
                rotate([90, 0, 0])
                    cylinder(h=CELL_L + 0.2, d=CELL_D, center=true);
        }

        // Centre divider slot for BMS board (1.2mm clearance)
        translate([0, 0, FLOOR + 1])
            cube([1.6, CELL_L * 0.7, CELL_D], center=true);
    }

    // Tabs to clip cradle into body (fit into body guide rails)
    for (sign = [-1, 1])
        translate([sign * (CRADLE_W/2 + 1), 0, 0])
            cube([2, 16, FLOOR + 3], center=true);

    // Wire exit notch labels
    for (sign = [-1, 1])
        translate([sign * (CRADLE_W/2 - 4), CRADLE_L/2 - 1, FLOOR + 3])
            rotate([90, 0, 0])
                linear_extrude(1.5)
                    text(sign > 0 ? "+" : "-", size=4,
                         halign="center", valign="center");
}

// ── PART 5: SENSOR BRACKET ───────────────────────────────────────────────────
module sensor_bracket() {
    // Mounts the US-100 sensor pointing down over the tank opening.
    // Designed to sit on the tank lid rim or bolt to a bracket bar.
    // US-100 snaps into a rectangular pocket on the underside.

    ARM_W  = 30;      // width of mounting arm
    ARM_L  = 80;      // total arm length
    ARM_T  = 5;       // arm thickness
    FOOT_W = 50;      // wider foot for stability
    FOOT_L = 20;
    FOOT_T = 5;

    // Sensor pocket dimensions (US-100 PCB: 44.5 × 20.5)
    PKT_W  = US_W + 0.4;   // 44.9
    PKT_D  = US_D + 0.4;   // 20.9
    PKT_H  = 8;            // depth of pocket

    // Mount holes (4mm, 35mm apart) for M4 bolts to tank bracket
    MOUNT_SPACING = 35;

    difference() {
        union() {
            // Mounting arm
            translate([0, -ARM_L/2 + FOOT_L, 0])
                cube([ARM_W, ARM_L, ARM_T], center=true);

            // Foot with mounting holes
            translate([0, (FOOT_L - ARM_L)/2 + FOOT_L/2, 0])
                cube([FOOT_W, FOOT_L, FOOT_T], center=true);

            // Sensor pocket housing (below arm, at far end)
            translate([0, ARM_L/2 - FOOT_L - PKT_W/2, -(PKT_H + ARM_T)/2])
                cube([PKT_D + 6, PKT_W + 4, PKT_H + ARM_T], center=true);
        }

        // Sensor pocket cutout
        translate([0, ARM_L/2 - FOOT_L - PKT_W/2, -(PKT_H + ARM_T)/2 + ARM_T])
            cube([PKT_D, PKT_W, PKT_H + 0.2], center=true);

        // US-100 cable hole (6mm) through arm center
        translate([0, ARM_L/2 - FOOT_L - PKT_W/2, 0])
            cylinder(h=ARM_T + PKT_H + 0.2, d=6, center=true);

        // Mounting holes in foot
        for (sign = [-1, 1])
            translate([sign * MOUNT_SPACING/2, (FOOT_L - ARM_L)/2 + FOOT_L/2, 0])
                cylinder(h=FOOT_T + 0.2, d=4.3, center=true);

        // Cable tie slots (2×5mm, 2 pairs on arm for cable management)
        for (dy = [-20, 20])
        for (dx = [-ARM_W/2 + 3, ARM_W/2 - 3])
            translate([dx, dy, 0])
                cube([2.2, 5, ARM_T + 0.2], center=true);
    }

    // Snap clips inside sensor pocket (flex tabs, 1mm wide)
    for (sign = [-1, 1])
        translate([sign * (PKT_D/2 - 0.5),
                   ARM_L/2 - FOOT_L - PKT_W/2,
                   -(PKT_H/2)])
            cube([1, 3, 2.5], center=true);
}

// ── RENDER SELECTION ─────────────────────────────────────────────────────────
// Comment/uncomment the part you want to export for slicing:

PART = "body";
// PART = "lid";
// PART = "pcb_tray";
// PART = "battery_cradle";
// PART = "sensor_bracket";
// PART = "all_exploded";

if (PART == "body")            enclosure_body();
else if (PART == "lid")        enclosure_lid();
else if (PART == "pcb_tray")   pcb_tray();
else if (PART == "battery_cradle") battery_cradle();
else if (PART == "sensor_bracket") sensor_bracket();
else if (PART == "all_exploded") {
    // Exploded view for checking fit — uncomment to see all parts together
    enclosure_body();
    translate([0, 0, ED/2 + LID_H + 10])  enclosure_lid();
    translate([0, 0, -ED/2 + WT + CELL_D + 12])  pcb_tray();
    translate([0, 0, -ED/2 + WT + 2])     battery_cradle();
    translate([EW + 20, 0, 0])            sensor_bracket();
}

