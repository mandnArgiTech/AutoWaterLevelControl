# FluidLevelMonitor

## Sintex Tank Water Level Monitoring System

A comprehensive IoT water level monitoring solution for circular/rectangular tanks using ESP8266 (NodeMCU) and US-100 ultrasonic sensor.

![Version](https://img.shields.io/badge/version-1.0.0-blue)
![Platform](https://img.shields.io/badge/platform-ESP8266-orange)
![License](https://img.shields.io/badge/license-MIT-green)

## Features

- **Water Level Monitoring**: Real-time water level measurement in percentage (0-100%)
- **Volume Calculation**: Calculates current water volume in liters
- **WiFi Connectivity**: Station mode with AP fallback for configuration
- **MQTT Publishing**: Publishes water level data in JSON format with timestamps
- **Web Interface**: Modern, responsive web UI for monitoring and configuration
- **REST API**: Complete API for remote access and integration
- **Configurable**: All settings stored in LittleFS and configurable via Web/REST
- **Error Handling**: Comprehensive error codes with descriptions
- **Auto Build Versioning**: Automatic build number increment on each compile

## Hardware Requirements

### Components

| Component | Description |
|-----------|-------------|
| NodeMCU V2 | ESP8266 development board |
| US-100 | Ultrasonic distance sensor (serial mode) |
| Jumper wires | For connections |
| Power supply | 5V USB or external |

### Wiring Diagram

```
NodeMCU V2          US-100
---------           ------
3.3V/5V    ------>  VCC
GND        ------>  GND
D1 (GPIO5) ------>  TX (Echo/RX)
D2 (GPIO4) ------>  RX (Trig/TX)

Note: Set jumper on US-100 for Serial Mode (UART)
```

### Default Tank Configuration (Sintex)

| Parameter | Value |
|-----------|-------|
| Type | Circular |
| Diameter | 1350 mm |
| Height | 1704.5 mm |
| Volume | ~2438 liters |

## Installation

### Prerequisites

- [PlatformIO](https://platformio.org/) (VSCode extension or CLI)
- Python 3.x (for build scripts)

### Build & Upload

1. **Clone or download the project**

2. **Build the firmware**:
   ```bash
   pio run
   ```

3. **Upload to NodeMCU**:
   ```bash
   pio run --target upload
   ```

4. **Upload LittleFS filesystem** (for web interface and configs):
   ```bash
   pio run --target uploadfs
   ```

5. **Monitor serial output**:
   ```bash
   pio device monitor
   ```

## Configuration

### First-Time Setup

1. Power on the device
2. Connect to WiFi AP: `FluidMonitor-AP` (password: `12345678`)
3. Open browser: `http://192.168.4.1`
4. Configure WiFi credentials
5. Device will restart and connect to your network

### Access Methods

| Method | URL |
|--------|-----|
| mDNS | `http://fluidmonitor.local` |
| IP Address | Check serial output or router |
| AP Mode | `http://192.168.4.1` |

## REST API

### Status Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/status` | GET | Device and level status |
| `/api/level` | GET | Current water level |
| `/api/sensor` | GET | Sensor status |
| `/api/info` | GET | Device information |

### Configuration Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/config` | GET | Full configuration |
| `/api/config` | POST | Update full configuration |
| `/api/config/wifi` | GET/POST | WiFi settings |
| `/api/config/mqtt` | GET/POST | MQTT settings |
| `/api/config/tank` | GET/POST | Tank dimensions |
| `/api/config/sensor` | GET/POST | Sensor calibration |
| `/api/config/system` | GET/POST | System settings |

### Network Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/wifi/status` | GET | WiFi status |
| `/api/wifi/scan` | GET | Scan available networks |
| `/api/mqtt/status` | GET | MQTT status |
| `/api/time` | GET | Time status |

### System Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/errors` | GET | Error history |
| `/api/errors/clear` | POST | Clear errors |
| `/api/restart` | POST | Restart device |
| `/api/reset` | POST | Factory reset |

### Example API Response

**GET /api/level**
```json
{
  "percentage": 75.5,
  "waterHeightMm": 1287,
  "volumeLiters": 1840.5,
  "distanceMm": 467.5,
  "temperatureC": 28,
  "valid": true,
  "state": "high",
  "timestamp": 125000
}
```

## MQTT

### Topics

| Topic | Description |
|-------|-------------|
| `home/water/level` | Water level data (JSON) |
| `home/water/status` | Device status (JSON) |
| `home/water/command` | Receive commands |

### MQTT Payload Example

```json
{
  "device": "Water Tank Monitor",
  "timestamp": "2024-01-15T10:30:00+05:30",
  "level": {
    "percentage": 75.5,
    "waterHeightMm": 1287,
    "volumeLiters": 1840.5,
    "state": "high"
  },
  "sensor": {
    "distanceMm": 468,
    "temperatureC": 28,
    "valid": true
  },
  "tank": {
    "type": "circular",
    "heightMm": 1704.5,
    "totalVolumeLiters": 2438.0
  }
}
```

### MQTT Commands

Send to `home/water/command`:
```json
{"command": "read"}      // Force reading
{"command": "status"}    // Publish status
{"command": "restart"}   // Restart device
```

## Project Structure

```
FluidLevelMonitor/
├── data/                    # LittleFS files
│   ├── config.json          # Configuration
│   ├── errors.json          # Error descriptions
│   └── index.html           # Web interface
├── scripts/
│   ├── build_increment.py   # Build version script
│   └── build_number.json    # Build counter
├── src/
│   ├── config/
│   │   ├── ConfigManager.h/.cpp
│   ├── network/
│   │   ├── WiFiManager.h/.cpp
│   │   ├── MQTTManager.h/.cpp
│   │   └── WebServer.h/.cpp
│   ├── sensor/
│   │   └── UltrasonicSensor.h/.cpp
│   ├── tank/
│   │   └── TankCalculator.h/.cpp
│   ├── utils/
│   │   ├── ErrorHandler.h/.cpp
│   │   └── TimeManager.h/.cpp
│   ├── version.h
│   └── main.cpp
├── platformio.ini
└── README.md
```

## Error Codes

| Range | Category |
|-------|----------|
| 0xx | System |
| 1xx | Sensor |
| 2xx | Tank |
| 3xx | WiFi |
| 4xx | MQTT |
| 5xx | Config/Storage |
| 6xx | Web Server |
| 7xx | Time |

See `data/errors.json` for complete error descriptions.

## Calibration

### Sensor Offset

The sensor offset is the distance from the sensor to the water surface when the tank is full:

1. Fill tank to maximum level
2. Note the sensor distance reading
3. Set this value as `offsetMm` in sensor configuration

### Tank Dimensions

For accurate volume calculation:
1. Measure actual tank dimensions
2. Update via web interface or API
3. Volume is auto-calculated

## Troubleshooting

| Issue | Solution |
|-------|----------|
| No sensor reading | Check wiring, verify serial mode jumper |
| WiFi not connecting | Verify credentials, check signal strength |
| MQTT not publishing | Verify broker settings, check network |
| Incorrect readings | Calibrate sensor offset |
| AP mode not starting | Factory reset, check for conflicts |

## License

MIT License - See LICENSE file for details.

## Contributing

Contributions welcome! Please submit pull requests or open issues.

---

**FluidLevelMonitor** - IoT Water Level Monitoring Made Easy

