# FluidLevelMonitor — Hardware Design

## Sensor Node: D1 Mini + US-100 + 32700 Battery Pack

> Document covers: power supply, battery charging, enclosure,
> wiring, firmware power optimisation, and BOM.

---

## 1. System Overview

```
┌─────────────────────────────────────────────────────────┐
│  WATERPROOF ENCLOSURE  (IP67 ABS, 115×90×55 mm)        │
│                                                          │
│  ┌──────────────┐    ┌──────────────────────────────┐  │
│  │  2× 32700    │    │  PCB (70×50 mm)              │  │
│  │  LiFePO4     │    │                              │  │
│  │  3.2V / 6Ah  │    │  TP5100    D1 Mini           │  │
│  │  cells       │    │  charger   ESP8266           │  │
│  │  (2S = 6.4V) │    │                              │  │
│  └──────┬───────┘    │  XL6009    US-100 header     │  │
│         │             │  booster   (cable outside)   │  │
│         │             └──────────────────────────────┘  │
│         └──────── power wiring ──────────────────────── │
│                                                          │
│  Charge port: IP67 USB-C on side panel                  │
│  US-100 cable: M12 cable gland on bottom                │
└─────────────────────────────────────────────────────────┘
```

---

## 2. Battery Selection — 32700 LiFePO4

### Why LiFePO4 (not Li-Ion)

| Property | LiFePO4 32700 | Li-Ion 18650 |
|----------|:---:|:---:|
| Nominal voltage | 3.2 V | 3.7 V |
| Full charge | 3.65 V | 4.2 V |
| Discharge cutoff | 2.5 V | 3.0 V |
| Cycle life | 2000+ | 500 |
| Thermal runaway risk | Negligible | Moderate |
| Safe in sealed enclosure | ✅ Yes | ⚠ Caution |
| Capacity (32700) | 6000 mAh | — |

LiFePO4 is the correct choice for a sealed outdoor enclosure. It will not expand, vent, or catch fire at 50°C ambient (Indian summer conditions).

### 2S Configuration (2 cells in series)

```
Cell 1 (+) ──┐
              ├── to XL6009 boost input (+)
Cell 1 (-)  ──┤
Cell 2 (+) ──┘
Cell 2 (−) ──── to common GND

Pack voltage:
  Discharged : 2× 2.5V = 5.0V
  Nominal    : 2× 3.2V = 6.4V
  Fully charged: 2× 3.65V = 7.3V
```

### Battery life estimate

| Component | Current draw |
|-----------|-------------|
| D1 Mini (WiFi active, transmitting) | ~170 mA peak, ~80 mA average |
| D1 Mini (WiFi active, idle) | ~70 mA |
| US-100 sensor (active) | ~3 mA |
| XL6009 booster (quiescent) | ~3 mA |
| TP5100 charger (standby) | ~1 mA |
| **Total average (always-on mode)** | **~80 mA** |

At 80 mA average from a 6 Ah pack: **~75 hours continuous**.

With 60-second publish interval, most of that time WiFi is idle (~30 mA):
**~180–200 hours ≈ 8 days between charges** (no solar).

With deep-sleep optimisation (see Section 7): **30+ days**.

---

## 3. Power Supply Design

### Challenge

The 2S LiFePO4 pack voltage varies from **5.0V to 7.3V**.
- D1 Mini needs **5V** on its 5V pin (it has an internal 3.3V LDO)
- US-100 runs on **3.3V or 5V** (we'll use 3.3V from D1 Mini's 3V3 pin)
- We must not feed the varying battery voltage directly — the D1 Mini's onboard LDO (RT9013, max 6V input) would be destroyed at 7.3V

### Solution: XL6009 Boost/Buck Regulator → 5V fixed

The XL6009 is a boost DC-DC converter. We use it in a slightly unusual way: the battery pack is already at 6–7.3V, which is **above** 5V, so we actually need a **buck (step-down)** converter, not a boost.

**Correct part: MP2307 / LM2596 / MP1584 buck module — 5V output, up to 10V input.**

```
Battery (5.0–7.3V) → MP1584 buck → 5V regulated → D1 Mini 5V pin
                                                   → US-100 VCC (via D1 Mini 3V3 out)
```

The MP1584 buck converter is:
- Input: 4.5–28V ✅ (handles full 32700 2S range)
- Output: adjustable, set to 5.0V
- Efficiency: ~92% at 100 mA
- Quiescent current: 0.6 mA
- Size: tiny — available as pre-built module (17×11 mm)

**Do not use an LDO (like AMS1117-5V)** — it requires input > 5V + dropout (~1.2V), so it would drop out at 6.2V leaving only a 1.3V margin. At high current it would also waste ~1.5V as heat.

---

## 4. Charging — TP5100 2S LiFePO4 Charger

### TP5100 module

The TP5100 is a linear charger IC designed for single or dual-cell Li-Ion/LiFePO4 with adjustable charge voltage.

For **2S LiFePO4**:
- Set charge voltage: **7.3V** (2 × 3.65V) via RPROG resistor
- Charge current: **1A** (set by Rprog = 1.2kΩ)
- Input: 5V USB-C → TP5100 input pin
- Output: directly to battery terminals

```
USB-C 5V ──► TP5100 ──► Battery (2S LiFePO4)
              │
              └── CHRG LED (charging indicator, optional)
              └── STDBY LED (full/standby indicator, optional)
```

**IMPORTANT:** The TP5100 must be configured for 2S LiFePO4, not 2S Li-Ion. The default modules from Amazon/AliExpress are often set for Li-Ion (8.4V). You must:
1. Check the Rprog resistor values on the board
2. Or buy the "2S LiFePO4" variant explicitly (7.3V output)
3. Or reconfigure by changing the voltage-divider resistors per TP5100 datasheet

Alternative: **CN3058E** — an affordable charger specifically designed for 2S LiFePO4 (7.3V fixed output, no resistor modification needed). Preferred if available.

### Charging via USB-C

A USB-C port with a PD decoy chip (e.g. CH224K) requests 5V from the charger. At 5V/1A that gives ~5W input to the TP5100. The TP5100 is a linear charger, so efficiency is limited — at 7.3V charge voltage from 5V input, it cannot work (input must be > output). 

**Revised approach: Use a USB-C PD input at 9V + TP5100 in 2S mode.**

```
USB-C PD (9V/1A) → TP5100 → 7.3V / 1A → 2× 32700 cells
                                │
                  also route 9V → MP1584 buck → 5V → D1 Mini
                  (charge and run simultaneously)
```

Use a **CH224K PD decoy chip** to request 9V from the USB-C adapter. Most modern USB-C phone chargers support 9V PD.

---

## 5. Complete Power Circuit

```
                    ┌────────────────────────────────────────┐
USB-C (9V PD)───────┤ CH224K (requests 9V from adapter)      │
                    │                                        │
                    │  9V ────┬──── TP5100 ──── 7.3V ────► Battery
                    │         │    (LiFePO4     (2S pack)
                    │         │     2S config)              │
                    │         │                              │
                    │         └──── MP1584 ──── 5V ─────► D1 Mini 5V pin
                    │               (buck)
                    └────────────────────────────────────────┘

                    When USB-C disconnected:
                    Battery (5.0–7.3V) ──── MP1584 ──── 5V ──► D1 Mini 5V pin

                    D1 Mini 3V3 out (3.3V, 500mA max) ──► US-100 VCC
                    D1 Mini D1 (GPIO5)  ──► US-100 RX  (UART mode)
                    D1 Mini D2 (GPIO4)  ──► US-100 TX  (UART mode)
                    GND ─────────────────── US-100 GND
```

### Battery protection

Add a **DW01A + FS8205A** 2S BMS module between battery and circuit:
- Overcurrent protection (short circuit)
- Overcharge protection (>3.65V/cell)
- Over-discharge protection (<2.5V/cell)
- Cell balancing (passive)

Small 2S BMS modules are available for ₹80–120.

---

## 6. D1 Mini Pinout for This Application

```
D1 Mini (ESP8266)
                    ┌──────────┐
                RST─┤          ├─TX  (Serial debug)
                 A0─┤          ├─RX  (Serial debug)
                 D0─┤          ├─D1  ← US-100 RX (SoftwareSerial)
                 D5─┤          ├─D2  ← US-100 TX (SoftwareSerial)
                 D6─┤          ├─D3  (available)
                 D7─┤          ├─D4  (built-in LED)
                 D8─┤          ├─GND
               3V3 ─┤          ├─5V  ← from MP1584 (5V regulated)
                    └──────────┘
          US-100 VCC ← 3V3 pin
```

**Why 3V3 for US-100:** The US-100 works at 3.3V–5V. Powering it from 3V3 (the D1 Mini's regulated 3.3V rail from its RT9013 LDO) eliminates a separate regulator and reduces idle current. At 3.3V the US-100 still reliably measures up to 4m.

**A0 pin (ADC):** Wire a resistor divider from the battery pack to A0 for battery voltage monitoring. The D1 Mini ADC reads 0–1V. Use a 100kΩ + 22kΩ divider:

```
Battery+ ──── 100kΩ ──── A0 ──── 22kΩ ──── GND

ADC reading × (100+22)/22 × 1.0V = battery voltage
At 6.4V nominal: A0 = 6.4 × 22/122 = 1.154V → use 100k+24k for 1V at 6.4V nominal
```

Actually use **100kΩ + 24kΩ** divider for better calibration:
- 7.3V (full) → A0 = 1.01V → ADC ≈ 1023
- 6.4V (nominal) → A0 = 0.885V → ADC ≈ 905
- 5.0V (cutoff) → A0 = 0.69V → ADC ≈ 707

---

## 7. Firmware — Battery Voltage Monitoring

Add to `ConfigManager` / sensor readings: publish `batteryVoltage` and `batteryPercent` in the MQTT payload.

```cpp
// In src/utils/BatteryMonitor.h
class BatteryMonitor {
public:
    static float readVoltage() {
        int raw = analogRead(A0);
        // Divider: 100k + 24k → scale = 124/24 = 5.1667
        float voltage = (raw / 1023.0f) * 1.0f * (124.0f / 24.0f);
        return voltage;
    }

    static uint8_t voltageToPercent(float v) {
        // 2S LiFePO4: 5.0V = 0%, 7.3V = 100%
        // Nearly flat discharge curve — use linear approximation
        if (v >= 7.3f) return 100;
        if (v <= 5.0f) return 0;
        return (uint8_t)((v - 5.0f) / (7.3f - 5.0f) * 100.0f);
    }

    static bool isLow(float v) { return v < 5.8f; }   // ~35%
    static bool isCritical(float v) { return v < 5.3f; } // ~13%
};
```

Add to MQTT JSON (`getMQTTJson`):
```json
{
  "level": { ... },
  "battery": {
    "voltage": 6.82,
    "percent": 79,
    "state": "good"    // "good" | "low" | "critical"
  }
}
```

---

## 8. Deep Sleep for Maximum Battery Life

The firmware currently runs always-on (WiFi up, polling every 2s). For battery operation, switch to deep sleep between publishes.

**Deep sleep cycle (60s publish interval):**

```
Wake → connect WiFi (~2s) → read sensor → publish MQTT → sleep 57s
```

Power breakdown:
- Active phase (~5s at 80 mA): 0.111 mAh
- Sleep phase (~55s at 20 µA): 0.000306 mAh
- Per cycle: ~0.111 mAh
- Per hour (60 cycles): ~6.7 mAh
- Battery life: 6000 mAh / 6.7 mAh = **895 hours ≈ 37 days**

Add to platformio.ini `sensor_only` env:
```ini
build_flags =
    -D FLM_ROLE_SENSOR
    -D FLM_DEEP_SLEEP_ENABLED     ; enable deep sleep mode
    -D FLM_SLEEP_SECONDS=57       ; sleep duration between readings
```

**D1 Mini deep sleep wiring:**
Connect `D0` (GPIO16) to `RST` with a wire. Deep sleep wakes by RST pulse from RTC timer.

```cpp
// At end of publish cycle:
#ifdef FLM_DEEP_SLEEP_ENABLED
    WiFi.disconnect(true);
    delay(100);
    ESP.deepSleep(FLM_SLEEP_SECONDS * 1000000UL);
#endif
```

---

## 9. Waterproof Enclosure

### Recommended box: Sonoff IP66 / Generic IP67 ABS 115×90×55mm

Key requirements:
- IP67 minimum (immersible to 1m — tank edge spray, not immersion)
- ABS plastic (not PC — PC degrades in UV without coating)
- Cable glands included or mountable

### Sealing

| Penetration | Seal method |
|-------------|-------------|
| US-100 cable | PG7 cable gland (M12 thread), IP68 rated |
| USB-C charge port | IP67 USB-C panel-mount connector (e.g. Amphenol UX series) or rubber blanking plug when not charging |
| Battery vent | None needed — LiFePO4 does not gas in normal operation |

### Mounting the US-100

The US-100 sensor head goes **outside** the enclosure, pointing down into the tank. The PCB and battery stay **inside**.

```
Enclosure wall (bottom)
        │
        ├── PG7 cable gland (sealed, IP68)
        │        │
        │   4-wire cable (VCC, GND, RX, TX)
        │        │
    US-100 sensor head
    (outside, pointing down into tank)
    
Mount sensor on a bracket above tank opening.
Minimum clearance to water surface when full: 25mm (US-100 blind zone).
Maximum range: 4500mm — covers all Sintex tank heights.
```

### Internal layout (115×90×55mm box)

```
┌─────────────────────────────────────┐
│  [Cell 1] [Cell 2]   (45×70mm area) │   ← 32700 cells side by side
│                                      │
│  [TP5100/CN3058E] [MP1584]           │   ← power modules
│  [BMS 2S]                            │
│                                      │
│  [D1 Mini on header]                 │   ← socketed for easy swap
└─────────────────────────────────────┘
```

32700 dimensions: ⌀32mm × 70mm — two cells fit side by side in 70×70mm footprint.

---

## 10. Bill of Materials

| # | Component | Spec | Qty | Approx cost (₹) |
|---|-----------|------|:---:|:---:|
| 1 | ESP8266 D1 Mini | v3, CH340 | 1 | 200 |
| 2 | US-100 Ultrasonic sensor | with jumper (UART mode) | 1 | 180 |
| 3 | 32700 LiFePO4 cell | 3.2V / 6000mAh | 2 | 600 |
| 4 | 2S LiFePO4 BMS | 6A, balancing, DW01A | 1 | 100 |
| 5 | CN3058E charger module | 2S LiFePO4, 7.3V out | 1 | 80 |
| 6 | CH224K PD decoy board | 9V trigger | 1 | 60 |
| 7 | MP1584 buck module | 4.5–28V in, 5V out, 3A | 1 | 60 |
| 8 | IP67 ABS enclosure | 115×90×55mm | 1 | 250 |
| 9 | PG7 cable gland | IP68, for US-100 4-wire | 1 | 30 |
| 10 | IP67 USB-C panel-mount | with cap | 1 | 120 |
| 11 | 100kΩ + 24kΩ resistors | 1% tolerance | 2 | 5 |
| 12 | 32700 cell holder | 2×32700 side by side | 1 | 50 |
| 13 | PCB / perfboard | 70×50mm | 1 | 30 |
| 14 | 4-wire cable | 22AWG, 30cm, for US-100 | 1 | 40 |
| 15 | Pin headers, wire, solder | — | — | 50 |
| **Total** | | | | **~₹1,855 per node** |

---

## 11. Assembly Sequence

1. **BMS wiring:** Connect two 32700 cells in series through BMS module. Verify pack voltage = 6.4V nominal at BMS output terminals.
2. **CH224K:** Wire to USB-C port. Verify 9V output with a USB-C PD adapter.
3. **CN3058E:** Input from CH224K 9V. Output to BMS B+ / B−. Test charge current with multimeter.
4. **MP1584:** Input from BMS output (Vm+ / Vm−). Adjust trim pot to 5.000V output. Verify with multimeter before connecting D1 Mini.
5. **Battery divider:** 100kΩ from battery Vm+ → A0, 24kΩ from A0 → GND. Verify `analogRead(A0)` ≈ 905 at 6.4V.
6. **D1 Mini:** Connect 5V from MP1584 to 5V pin. GND to GND. Verify 3V3 pin = 3.28–3.32V.
7. **US-100:** VCC to 3V3, GND to GND, RX to D1 (GPIO5), TX to D2 (GPIO4). Jumper on sensor = UART mode.
8. **Deep sleep wire:** Thin wire from D0 to RST (only if using deep sleep firmware).
9. **Test outside enclosure** — flash firmware, verify MQTT publishes with correct distance + battery voltage.
10. **Seal and mount** — Route US-100 cable through PG7 gland, seal with silicone around gland thread. Press gasket, close lid, torque screws evenly.

---

## 12. Safety Notes

- Never charge LiFePO4 below 0°C (outdoor winter — add foam insulation inside enclosure)
- The BMS protects against shorts and overcharge, but test it before sealing
- Apply silicone sealant (RTV, clear) around all cable gland threads from inside the enclosure for belt-and-suspenders IP67 seal
- Label each enclosure with tank name (farm_tank1, home_sump etc.) and WiFi hostname before sealing
- Keep a 3mm hex key taped to the enclosure lid for field opening

