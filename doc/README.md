# FluidLevelMonitor Documentation

## Overview

FluidLevelMonitor is a robust, modular IoT water level monitoring system designed for Sintex tanks and similar containers. Built for NodeMCU v2 (ESP8266), it provides accurate water level measurements with comprehensive filtering for stable readings.

## Documentation Index

| Document | Description | Audience |
|----------|-------------|----------|
| [User Guide](USER_GUIDE.md) | Setup, installation, and daily usage | End Users |
| [Developer Guide](DEVELOPER_GUIDE.md) | Architecture, code structure, extending | Developers |
| [API Reference](API_REFERENCE.md) | REST, MQTT, WebSocket, limits | Integrators |
| [Architecture](ARCHITECTURE.md) | Boot, modules, OTA, logging, threat model | Developers |
| [Code style](CODE_STYLE.md) | Comments, logging policy, secrets | Developers |
| [Manual test matrix](MANUAL_TEST_MATRIX.md) | QA checklist, `pio check` | Developers |
| [Sensor Guide](SENSOR_GUIDE.md) | Supported sensors and selection | Everyone |
| [Troubleshooting](TROUBLESHOOTING.md) | Common issues and solutions | Everyone |
| [Project status](PROJECT-STATUS.md) | Completion, goals, M1/M2/M3, next steps | Everyone |
| [Deploy & OTA](DEPLOY-AND-OTA.md) | Remote VPS redeploy + USB/OTA firmware burn | Operators / Devs |
| [Backend redesign](BACKEND-REDESIGN.md) | Platform architecture vision | Developers |
| [Backend stories](BACKEND-REDESIGN-STORIES.md) | Story backlog (checkboxes may lag status file) | Developers |

## Key Features

### 📊 Measurement
- Water level as percentage (filled and remaining)
- Water height in centimeters
- Volume in liters
- Real-time updates via WebSocket

### 🔌 Sensor Support
- **US-100** - Ultrasonic (UART, with temperature)
- **HC-SR04** - Ultrasonic (Trigger/Echo)
- **TF-Luna** - LiDAR (high precision)
- **XKC-KD200** - IR point-level (through-wall)

### 📡 Connectivity
- WiFi with AP fallback mode
- MQTT publishing (JSON format)
- REST API for integration
- WebSocket for real-time data
- OTA firmware updates

### 🛠️ Configuration
- Web-based configuration interface
- All settings stored in LittleFS
- No hardcoded credentials

### 🔧 Signal Processing
- **Median Filter** - Eliminates spikes
- **Moving Average** - Smooths ripples
- **Kalman Filter** - Optimal estimation

## Quick Start

1. Flash firmware to NodeMCU
2. Upload filesystem image (LittleFS)
3. Connect to `FluidLM_XXXXXX` WiFi
4. Open `http://192.168.4.1`
5. Configure WiFi and tank settings
6. Mount sensor on tank

## Hardware Requirements

- NodeMCU v2 (ESP8266)
- Distance sensor (US-100 recommended)
- 5V power supply
- Mounting hardware

## Default Configuration

| Setting | Default Value |
|---------|---------------|
| Tank Type | Circular |
| Diameter | 1350 mm |
| Height | 1704.5 mm |
| Sensor Type | US-100 |
| RX Pin | D1 (GPIO5) |
| TX Pin | D2 (GPIO4) |

## License

MIT License - See LICENSE file

## Version

Current: 1.0.0

---

*For detailed information, please refer to the specific documentation files listed above.*

