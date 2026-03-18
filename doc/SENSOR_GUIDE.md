# FluidLevelMonitor - Sensor Guide

## Table of Contents
1. [Sensor Comparison](#sensor-comparison)
2. [US-100 Ultrasonic](#us-100-ultrasonic)
3. [HC-SR04 Ultrasonic](#hc-sr04-ultrasonic)
4. [TF-Luna LiDAR](#tf-luna-lidar)
5. [XKC-KD200 IR](#xkc-kd200-ir)
6. [Selection Guide](#selection-guide)
7. [Filter Configuration](#filter-configuration)

---

## Sensor Comparison

| Feature | US-100 | HC-SR04 | TF-Luna | XKC-KD200 |
|---------|--------|---------|---------|-----------|
| **Technology** | Ultrasonic | Ultrasonic | LiDAR (ToF) | Infrared |
| **Range** | 2-450 cm | 2-400 cm | 20-800 cm | Point-level |
| **Resolution** | 1 mm | 3 mm | 10 mm | N/A |
| **Accuracy** | ±3 mm | ±3 mm | ±6 cm | Detection only |
| **Interface** | UART | Trigger/Echo | UART | Digital |
| **Voltage** | 2.4-5.5V | 5V | 5V | 5-24V |
| **Temperature** | Yes | No | Yes | No |
| **Price** | $ | $ | $$$ | $ |
| **Best For** | General use | Budget | Precision | Through-wall |

---

## US-100 Ultrasonic

### Overview

The US-100 is a versatile ultrasonic sensor with UART interface and built-in temperature compensation.

### Specifications

| Parameter | Value |
|-----------|-------|
| Working Voltage | 2.4V - 5.5V DC |
| Working Current | 2mA |
| Measuring Range | 2cm - 450cm |
| Resolution | 1mm |
| Accuracy | ±3mm + 1% |
| Beam Angle | 15° |
| Interface | UART (9600 baud) |
| Temperature Range | -20°C to +70°C |

### Wiring (UART Mode)

```
NodeMCU          US-100
--------         ------
5V/VIN  -------> VCC
GND     -------> GND
D1 (RX) -------> TX
D2 (TX) -------> RX
```

**Important:** Set the jumper on the US-100 to enable UART mode!

### Configuration

```json
{
  "sensor": {
    "hardware": {
      "type": "US100",
      "rxPin": 5,
      "txPin": 4
    }
  }
}
```

### Advantages
- ✅ Built-in temperature compensation
- ✅ UART interface (noise resistant)
- ✅ Wide voltage range
- ✅ Good accuracy

### Disadvantages
- ❌ Affected by humidity
- ❌ Narrow beam (may miss edges)
- ❌ Sensitive to foam/bubbles

### Best Practices
1. Mount perpendicular to water surface
2. Keep away from walls (>5cm)
3. Avoid direct sunlight on sensor face
4. Use filters for ripple compensation

---

## HC-SR04 Ultrasonic

### Overview

The HC-SR04 is the most common and affordable ultrasonic sensor, using trigger/echo pulse timing.

### Specifications

| Parameter | Value |
|-----------|-------|
| Working Voltage | 5V DC |
| Working Current | 15mA |
| Measuring Range | 2cm - 400cm |
| Resolution | ~3mm |
| Accuracy | ±3mm |
| Beam Angle | 15° |
| Interface | Trigger/Echo |
| Trigger Pulse | 10µs |

### Wiring

```
NodeMCU          HC-SR04
--------         -------
5V/VIN  -------> VCC
GND     -------> GND
D1 (GPIO5) ----> TRIG
D2 (GPIO4) <---- ECHO (via voltage divider!)
```

**⚠️ IMPORTANT:** The ECHO pin outputs 5V! Use a voltage divider:

```
                    ┌─── 1kΩ ───┬─── to NodeMCU D2
HC-SR04 ECHO ───────┤           │
                    └─── 2kΩ ───┴─── GND
```

### Configuration

```json
{
  "sensor": {
    "hardware": {
      "type": "HC_SR04",
      "trigPin": 5,
      "echoPin": 4
    }
  }
}
```

### Advantages
- ✅ Very affordable
- ✅ Widely available
- ✅ Simple interface

### Disadvantages
- ❌ No temperature compensation
- ❌ 5V logic requires level shifting
- ❌ Affected by humidity/temperature
- ❌ Less accurate than US-100

### Temperature Compensation

Since HC-SR04 lacks temperature sensing, you can manually set temperature:

```cpp
HCSR04Sensor* sensor = ...;
sensor->setTemperature(25.0); // Set ambient temperature
```

---

## TF-Luna LiDAR

### Overview

The TF-Luna is a high-precision LiDAR (Light Detection and Ranging) sensor using Time-of-Flight technology.

### Specifications

| Parameter | Value |
|-----------|-------|
| Working Voltage | 5V DC |
| Working Current | 70mA |
| Measuring Range | 20cm - 800cm |
| Resolution | 1cm |
| Accuracy | ±6cm (0.2-3m), ±2% (3-8m) |
| Frame Rate | 1-250 Hz |
| Interface | UART (115200 baud) |
| FOV | 2° |

### Wiring

```
NodeMCU          TF-Luna
--------         -------
5V/VIN  -------> VCC (5V)
GND     -------> GND
D1 (RX) -------> TX
D2 (TX) -------> RX
```

### Configuration

```json
{
  "sensor": {
    "hardware": {
      "type": "TF_LUNA",
      "rxPin": 5,
      "txPin": 4
    }
  }
}
```

### Advantages
- ✅ Very precise
- ✅ Long range (8 meters)
- ✅ Narrow beam (2°) - accurate spot measurement
- ✅ Works in any light conditions
- ✅ Not affected by humidity
- ✅ Built-in temperature sensor

### Disadvantages
- ❌ More expensive
- ❌ May miss small targets (narrow beam)
- ❌ Can be affected by transparent surfaces
- ❌ Higher power consumption

### Best Applications
- Deep tanks (>2 meters)
- Outdoor installations
- High precision requirements
- Industrial applications

### Signal Strength

The TF-Luna provides signal strength data. Weak signals indicate:
- Target too far
- Transparent or reflective surface
- Dirty sensor lens

---

## XKC-KD200 IR

### Overview

The XKC-KD200 is a non-contact infrared liquid level sensor that detects liquid presence through container walls.

### Specifications

| Parameter | Value |
|-----------|-------|
| Working Voltage | 5-24V DC |
| Working Current | <5mA |
| Detection | Through walls up to 20mm |
| Output | NPN Open Collector |
| Response Time | 500ms |
| Interface | Digital (HIGH/LOW) |

### Wiring

```
NodeMCU          XKC-KD200
--------         ---------
5V/VIN  -------> VCC (red)
GND     -------> GND (black)
D5 (GPIO14) <--- OUT (yellow)
```

### Configuration

```json
{
  "sensor": {
    "hardware": {
      "type": "XKC_KD200",
      "signalPin": 14,
      "mountHeightMm": 500,
      "tankHeightMm": 1704.5,
      "invertedLogic": false
    }
  }
}
```

### Operation

This is a **point-level** sensor, not a continuous distance sensor:

- **HIGH output**: Liquid detected at sensor position
- **LOW output**: No liquid at sensor position

### Level Calculation

Since it only detects presence at mount height:

```
If liquid detected:
  → Water level = mount height
  → Distance from top = tankHeight - mountHeight

If no liquid:
  → Water below mount height
  → Reports maximum distance (tank height)
```

### Advantages
- ✅ Works through tank wall (non-invasive)
- ✅ Not affected by tank contents
- ✅ No contact with liquid
- ✅ Simple installation
- ✅ Very reliable

### Disadvantages
- ❌ Only detects one level (point sensor)
- ❌ Must be mounted externally
- ❌ Limited to specific wall thicknesses
- ❌ No continuous measurement

### Multi-Sensor Setup

For better resolution, use multiple XKC-KD200 sensors at different heights:

```
     ┌─────────────────────┐
     │ [SENSOR 3] - 90%    │
     │                     │
     │ [SENSOR 2] - 50%    │
     │                     │
     │ [SENSOR 1] - 20%    │
     └─────────────────────┘
```

---

## Selection Guide

### Decision Tree

```
                        ┌─────────────────────┐
                        │ What's your budget? │
                        └──────────┬──────────┘
                                   │
              ┌────────────────────┼────────────────────┐
              ▼                    ▼                    ▼
          Low ($)            Medium ($$)          High ($$$)
              │                    │                    │
              ▼                    ▼                    ▼
        ┌─────────┐          ┌─────────┐          ┌─────────┐
        │ HC-SR04 │          │ US-100  │          │ TF-Luna │
        └─────────┘          └─────────┘          └─────────┘
              │                    │                    │
              ▼                    ▼                    ▼
       Budget option        Recommended          Maximum precision
       Basic accuracy       Good all-round       Long range
       5V level shift       UART interface       8m range
       needed               Temp compensation    Industrial grade
```

### Use Case Recommendations

| Scenario | Recommended | Alternative |
|----------|-------------|-------------|
| **Home tank (indoor)** | US-100 | HC-SR04 |
| **Outdoor installation** | TF-Luna | US-100 |
| **Deep tank (>2m)** | TF-Luna | US-100 |
| **Budget project** | HC-SR04 | - |
| **Through-wall sensing** | XKC-KD200 | - |
| **Industrial application** | TF-Luna | US-100 |
| **Transparent tank** | XKC-KD200 | TF-Luna |
| **Foam on surface** | XKC-KD200 | TF-Luna |

---

## Filter Configuration

All distance sensors (US-100, HC-SR04, TF-Luna) use a 3-stage filter pipeline:

### Filter Pipeline

```
Raw Reading → [Median] → [Moving Avg] → [Kalman] → Output
               ↓            ↓             ↓
         Remove spikes  Smooth ripples  Optimal estimate
```

### Configuration Options

```json
{
  "sensor": {
    "filterEnabled": true,
    "medianFilterSize": 5,
    "movingAvgWindow": 10,
    "kalmanEnabled": true,
    "kalmanProcessNoise": 0.01,
    "kalmanMeasureNoise": 0.1
  }
}
```

### Filter Tuning Guide

#### Median Filter
- **Size 3**: Minimum spike rejection
- **Size 5**: Recommended (default)
- **Size 7-9**: More aggressive spike removal

#### Moving Average
- **Window 5**: Fast response, some smoothing
- **Window 10**: Recommended (default)
- **Window 15-20**: Very smooth but slow response

#### Kalman Filter

| Q (Process) | R (Measure) | Result |
|-------------|-------------|--------|
| 0.001 | 0.5 | Maximum smoothing, slow |
| 0.01 | 0.1 | Balanced (default) |
| 0.1 | 0.05 | Fast response, less smooth |

### Scenario-Based Settings

**Calm Water (indoor tank):**
```json
{
  "medianFilterSize": 3,
  "movingAvgWindow": 5,
  "kalmanProcessNoise": 0.001,
  "kalmanMeasureNoise": 0.1
}
```

**Agitated Water (pumps, wind):**
```json
{
  "medianFilterSize": 7,
  "movingAvgWindow": 15,
  "kalmanProcessNoise": 0.01,
  "kalmanMeasureNoise": 0.5
}
```

**Fast Fill/Drain:**
```json
{
  "medianFilterSize": 5,
  "movingAvgWindow": 5,
  "kalmanProcessNoise": 0.1,
  "kalmanMeasureNoise": 0.05
}
```

---

## Troubleshooting

### Common Issues

| Problem | Possible Cause | Solution |
|---------|----------------|----------|
| No reading | Wrong wiring | Check connections |
| Timeout errors | Bad connection | Verify pins, check jumper (US-100) |
| Erratic readings | EMI interference | Use shielded cables |
| Too much variation | Ripples/waves | Increase filter settings |
| Readings too slow | Over-filtering | Reduce filter windows |
| Zero distance | Sensor blocked | Clean sensor surface |
| Max distance | Target too far | Use different sensor |

---

*For more details, see the [Developer Guide](DEVELOPER_GUIDE.md) and [Troubleshooting](TROUBLESHOOTING.md).*

