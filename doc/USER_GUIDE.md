# FluidLevelMonitor - User Guide

## Table of Contents
1. [Introduction](#introduction)
2. [Hardware Setup](#hardware-setup)
3. [Initial Configuration](#initial-configuration)
4. [Web Interface](#web-interface)
5. [Tank Configuration](#tank-configuration)
6. [Sensor Calibration](#sensor-calibration)
7. [MQTT Setup](#mqtt-setup)
8. [Maintenance](#maintenance)

---

## Introduction

FluidLevelMonitor measures water level in your tank and reports it as:
- **Percentage filled** (e.g., 75% full)
- **Percentage remaining** (e.g., 25% to fill)
- **Water height** in centimeters
- **Volume** in liters

The system can publish data to MQTT brokers for home automation integration (Home Assistant, Node-RED, etc.).

---

## Hardware Setup

### Components Required

| Component | Purpose | Notes |
|-----------|---------|-------|
| NodeMCU v2 | Main controller | ESP8266 based |
| US-100 Sensor | Distance measurement | Recommended |
| 5V Power Supply | Power | 1A minimum |
| Mounting bracket | Sensor mounting | Waterproof enclosure recommended |

### Wiring Diagram (US-100)

```
NodeMCU          US-100 Sensor
--------         -------------
5V/VIN  -------> VCC
GND     -------> GND
D1 (GPIO5) ----> RX (Echo)
D2 (GPIO4) ----> TX (Trig)
```

**Important**: Set the jumper on US-100 to enable UART mode.

### Wiring Diagram (HC-SR04)

```
NodeMCU          HC-SR04 Sensor
--------         --------------
5V/VIN  -------> VCC
GND     -------> GND
D1 (GPIO5) ----> TRIG
D2 (GPIO4) ----> ECHO*

*Use voltage divider (5V to 3.3V) on ECHO!
```

### Sensor Mounting

1. Mount sensor at the **top of the tank** pointing downward
2. Ensure sensor is **level** (perpendicular to water surface)
3. Keep sensor **away from tank walls** (minimum 5cm)
4. Protect sensor from **water splashes** and **condensation**
5. Use waterproof enclosure for outdoor installations

```
    ┌─────────────────────┐
    │     [SENSOR]        │ ← Mount here, pointing down
    │         ↓           │
    │    ~~~~~~~~~~~~     │ ← Water surface
    │    │          │     │
    │    │  WATER   │     │
    │    │          │     │
    └────┴──────────┴─────┘
```

---

## Initial Configuration

### First Boot - AP Mode

When first powered on (or if WiFi connection fails), the device enters **Access Point Mode**:

1. **Connect to WiFi network**: `FluidLM_XXXXXX` (where XXXXXX is device ID)
2. **Password**: `12345678`
3. **Open browser**: `http://192.168.4.1`

### WiFi Configuration

1. Navigate to **Settings → WiFi**
2. Enter your home WiFi credentials:
   - **SSID**: Your WiFi network name
   - **Password**: Your WiFi password
3. Click **Save**
4. Device will restart and connect to your network

### Finding Device IP

After connecting to your network:
- Check your router's DHCP client list
- Use mDNS: `http://fluidmonitor.local`
- Serial monitor shows IP on boot

---

## Web Interface

### Dashboard

The main dashboard shows:

```
┌─────────────────────────────────────┐
│         WATER LEVEL: 75%            │
│         ████████████░░░░            │
│                                     │
│  Filled:     75.2%                  │
│  Remaining:  24.8%                  │
│  Height:     128.0 cm               │
│  Volume:     1832 L                 │
│  Status:     NORMAL                 │
└─────────────────────────────────────┘
```

### Navigation

| Tab | Description |
|-----|-------------|
| **Dashboard** | Current water level display |
| **Settings** | Configuration options |
| **Calibration** | Sensor calibration tool |
| **System** | Device info and maintenance |

---

## Tank Configuration

### Circular Tanks (Sintex)

1. Go to **Settings → Tank**
2. Select Type: **Circular**
3. Enter dimensions:
   - **Diameter**: Tank diameter in mm (e.g., 1350)
   - **Height**: Tank height in mm (e.g., 1704.5)
4. Click **Save**

### Rectangular Tanks

1. Select Type: **Rectangular**
2. Enter dimensions:
   - **Length**: Tank length in mm
   - **Width**: Tank width in mm
   - **Height**: Tank height in mm
3. Click **Save**

### Common Sintex Tank Sizes

| Model | Diameter (mm) | Height (mm) | Volume (L) |
|-------|---------------|-------------|------------|
| 500L  | 900           | 980         | 500        |
| 1000L | 1100          | 1320        | 1000       |
| 2000L | 1350          | 1704        | 2000       |
| 3000L | 1550          | 1990        | 3000       |

---

## Sensor Calibration

### Why Calibrate?

The sensor measures distance from its face to the water. Calibration accounts for:
- Distance from sensor to tank top
- Sensor mounting offset
- Any systematic errors

### Calibration Steps

1. Go to **Calibration** tab
2. Fill tank to a **known level** (e.g., empty or measure manually)
3. Note the **measured distance** shown
4. Enter the **actual distance** (in cm)
5. Click **Calibrate**

### Real-Time Calibration (WebSocket)

The calibration page shows real-time distance readings:
- Use this to verify sensor is working
- Readings update every 200ms
- Useful for troubleshooting mounting issues

---

## MQTT Setup

### Configuration

1. Go to **Settings → MQTT**
2. Enter broker details:
   - **Enabled**: Check to enable
   - **Server**: MQTT broker IP/hostname
   - **Port**: Usually 1883
   - **Username**: (if required)
   - **Password**: (if required)
   - **Topic Prefix**: e.g., `home/water`
3. Click **Save**

### Published Topics

| Topic | Description | Format |
|-------|-------------|--------|
| `{prefix}/level` | Water level data | JSON |
| `{prefix}/status` | Device status | JSON |

### Message Format (Level)

```json
{
  "timestamp": "2024-01-15T10:30:00+05:30",
  "percentFilled": 75.2,
  "percentRemaining": 24.8,
  "waterHeightCm": 128.0,
  "distanceCm": 42.5,
  "volumeLiters": 1832.5,
  "volumeRemaining": 605.5,
  "state": "NORMAL",
  "valid": true
}
```

### Home Assistant Integration

```yaml
# configuration.yaml
mqtt:
  sensor:
    - name: "Water Tank Level"
      state_topic: "home/water/level"
      value_template: "{{ value_json.percentFilled }}"
      unit_of_measurement: "%"
      
    - name: "Water Tank Volume"
      state_topic: "home/water/level"
      value_template: "{{ value_json.volumeLiters }}"
      unit_of_measurement: "L"
```

---

## Maintenance

### Firmware Updates (OTA)

1. Go to **System → Update**
2. Upload new `firmware.bin` file
3. Wait for update to complete (~30 seconds)
4. Device will restart automatically

### Factory Reset

1. Go to **System → Reset**
2. Click **Factory Reset**
3. Confirm action
4. Device returns to AP mode with default settings

### Backup/Restore Configuration

**Export Settings:**
1. Go to **System → Backup**
2. Click **Export**
3. Save JSON file

**Import Settings:**
1. Click **Import**
2. Select saved JSON file
3. Configuration is restored

### Recommended Maintenance

| Task | Frequency |
|------|-----------|
| Check sensor mounting | Monthly |
| Clean sensor surface | As needed |
| Verify calibration | Quarterly |
| Update firmware | When available |

---

## LED Status Indicators

| Pattern | Meaning |
|---------|---------|
| Solid ON | Connected to WiFi |
| Slow blink | AP mode active |
| Fast blink | Connecting to WiFi |
| Off | No power / Error |

---

## Specifications

| Parameter | Value |
|-----------|-------|
| Input Voltage | 5V DC |
| Current Draw | ~150mA (WiFi active) |
| WiFi | 802.11 b/g/n |
| Range (US-100) | 2cm - 450cm |
| Accuracy | ±3mm |
| Update Rate | 500ms (configurable) |

---

*For technical details, see the [Developer Guide](DEVELOPER_GUIDE.md).*

