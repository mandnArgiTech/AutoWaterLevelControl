# FluidLevelMonitor - Developer Guide

## Table of Contents
1. [Architecture Overview](#architecture-overview)
2. [Project Structure](#project-structure)
3. [Module Descriptions](#module-descriptions)
4. [Design Patterns](#design-patterns)
5. [Adding New Sensors](#adding-new-sensors)
6. [Build System](#build-system)
7. [Code Conventions](#code-conventions)
8. [Testing](#testing)

---

## Architecture Overview

### System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        APPLICATION LAYER                         │
│  ┌─────────┐  ┌──────────┐  ┌─────────────┐  ┌──────────────┐  │
│  │ main.cpp│  │WebServer │  │CalibrationWS│  │ MQTTManager  │  │
│  └────┬────┘  └────┬─────┘  └──────┬──────┘  └──────┬───────┘  │
├───────┼────────────┼───────────────┼────────────────┼───────────┤
│       │            │               │                │           │
│       ▼            ▼               ▼                ▼           │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                    BUSINESS LOGIC                        │   │
│  │  ┌───────────────┐        ┌─────────────────────────┐   │   │
│  │  │ TankCalculator│◄──────►│       ISensor           │   │   │
│  │  │               │        │  (Abstract Interface)   │   │   │
│  │  └───────────────┘        └───────────┬─────────────┘   │   │
│  │                                       │                  │   │
│  │         ┌─────────────────────────────┼──────────────┐  │   │
│  │         ▼              ▼              ▼              ▼  │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────┐ │   │
│  │  │UltraSonic│  │ HCSR04   │  │ TFLuna   │  │XKCKD200 │ │   │
│  │  │ (US-100) │  │          │  │ (LiDAR)  │  │  (IR)   │ │   │
│  │  └──────────┘  └──────────┘  └──────────┘  └─────────┘ │   │
│  └─────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│                       INFRASTRUCTURE                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐ │
│  │ConfigManager│  │ WiFiManager │  │     TimeManager         │ │
│  │  (LittleFS) │  │  (OTA/mDNS) │  │       (NTP)             │ │
│  └─────────────┘  └─────────────┘  └─────────────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                         UTILITIES                                │
│  ┌─────────────┐  ┌─────────────────────────────────────────┐  │
│  │ErrorHandler │  │            SensorFilter                  │  │
│  │             │  │  [Median] → [MovingAvg] → [Kalman]      │  │
│  └─────────────┘  └─────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### Data Flow

```
Sensor → Filter Pipeline → TankCalculator → Output (MQTT/REST/WebSocket)
           │
           ├── MedianFilter (spike removal)
           ├── MovingAverageFilter (ripple smoothing)
           └── KalmanFilter (optimal estimation)
```

---

## Project Structure

```
FluidLevelMonitor/
├── src/
│   ├── main.cpp                 # Application entry point
│   ├── version.h                # Version macros
│   │
│   ├── config/
│   │   ├── ConfigManager.h      # Configuration singleton
│   │   └── ConfigManager.cpp
│   │
│   ├── sensor/
│   │   ├── ISensor.h            # Abstract sensor interface
│   │   ├── SensorFactory.h/cpp  # Factory for creating sensors
│   │   ├── SensorFilter.h/cpp   # Median, MovingAvg, Kalman filters
│   │   ├── UltrasonicSensor.h/cpp   # US-100 implementation
│   │   ├── HCSR04Sensor.h/cpp       # HC-SR04 implementation
│   │   ├── TFLunaSensor.h/cpp       # TF-Luna LiDAR
│   │   └── XKCKD200Sensor.h/cpp     # IR point-level sensor
│   │
│   ├── tank/
│   │   ├── TankCalculator.h     # Water level calculations
│   │   └── TankCalculator.cpp
│   │
│   ├── network/
│   │   ├── WiFiManager.h/cpp    # WiFi, AP mode, OTA
│   │   ├── MQTTManager.h/cpp    # MQTT publishing
│   │   ├── WebServer.h/cpp      # REST API, web interface
│   │   └── CalibrationWS.h/cpp  # WebSocket for calibration
│   │
│   └── utils/
│       ├── ErrorHandler.h/cpp   # Error codes and logging
│       └── TimeManager.h/cpp    # NTP time synchronization
│
├── data/                        # LittleFS filesystem
│   ├── config.json              # Default configuration
│   ├── errors.json              # Error code definitions
│   └── index.html               # Web interface
│
├── scripts/
│   ├── build_increment.py       # Auto-increment build number
│   └── build_number.json        # Current build number
│
├── doc/                         # Documentation
│   ├── README.md
│   ├── USER_GUIDE.md
│   ├── DEVELOPER_GUIDE.md
│   ├── API_REFERENCE.md
│   └── SENSOR_GUIDE.md
│
└── platformio.ini               # PlatformIO configuration
```

---

## Module Descriptions

### ConfigManager (Singleton)

Manages all configuration persistence using LittleFS.

```cpp
// Get instance
ConfigManager& config = ConfigManager::getInstance();

// Access configuration
WiFiConfig& wifi = config.getWiFiConfig();
TankConfig& tank = config.getTankConfig();
SensorConfig& sensor = config.getSensorConfig();

// Save/load
config.saveConfig();
config.loadConfig();
```

### ISensor (Interface)

Abstract interface all sensors must implement:

```cpp
class ISensor {
public:
    virtual ErrorCode begin() = 0;
    virtual bool isReady() const = 0;
    virtual float readDistanceMm() = 0;
    virtual float readDistanceAverageMm(uint8_t samples) = 0;
    virtual SensorReading getReading() = 0;
    virtual SensorType getSensorType() const = 0;
    virtual String getSensorTypeName() const = 0;
    virtual float getMinRange() const = 0;
    virtual float getMaxRange() const = 0;
    virtual String getStatusJson() const = 0;
    // ...
};
```

### SensorFactory

Creates sensor instances based on configuration:

```cpp
// From hardware config
ISensor* sensor = SensorFactory::createSensor(sensorHWConfig);

// From type string
ISensor* sensor = SensorFactory::createSensorByType("TF_LUNA");

// Supported types
String types = SensorFactory::getSupportedTypes(); // JSON array
```

### FluidLevelFilter

Three-stage filtering pipeline:

```cpp
FluidLevelFilter filter(5, 10, 0.01f, 0.1f);
//                      │   │   │      └── Kalman R (measurement noise)
//                      │   │   └── Kalman Q (process noise)
//                      │   └── Moving average window
//                      └── Median filter size

float filtered = filter.filter(rawReading);
```

### TankCalculator

Converts sensor readings to water level:

```cpp
TankCalculator calculator(sensor);
calculator.begin();

WaterLevel level = calculator.calculate();
// level.percentFilled    - 75.5%
// level.percentRemaining - 24.5%
// level.waterHeightCm    - 128.4 cm
// level.volumeLiters     - 1845.2 L
```

---

## Design Patterns

### 1. Singleton Pattern
Used for global managers:
- `ConfigManager::getInstance()`
- `WiFiManager::getInstance()`
- `MQTTManager::getInstance()`
- `TimeManager::getInstance()`
- `ErrorHandler::getInstance()`

### 2. Factory Pattern
`SensorFactory` creates appropriate sensor based on configuration.

### 3. Strategy Pattern
`ISensor` interface allows runtime sensor selection.

### 4. Observer Pattern
WiFi and MQTT state callbacks:
```cpp
WiFiManager::getInstance().setStateCallback(handleWiFiStateChange);
MQTTManager::getInstance().setMessageCallback(handleMQTTMessage);
```

---

## Adding New Sensors

### Step 1: Create Header File

```cpp
// src/sensor/MySensor.h
#ifndef MY_SENSOR_H
#define MY_SENSOR_H

#include "ISensor.h"
#include "SensorFilter.h"

class MySensor : public ISensor {
public:
    MySensor(int pin1, int pin2);
    
    // Implement ISensor interface
    ErrorCode begin() override;
    bool isReady() const override;
    float readDistanceMm() override;
    float readDistanceAverageMm(uint8_t samples) override;
    SensorReading getReading() override;
    const SensorReading& getLastReading() const override;
    SensorType getSensorType() const override;
    String getSensorTypeName() const override;
    ErrorCode getLastError() const override;
    float getMinRange() const override;
    float getMaxRange() const override;
    bool testConnection() override;
    String getStatusJson() const override;
    
    // Add filter support
    FluidLevelFilter& getFilter() { return _filter; }
    void setFilterEnabled(bool enable);
    
private:
    FluidLevelFilter _filter;
    SensorReading _lastReading;
    // ... other members
};

#endif
```

### Step 2: Implement Sensor

```cpp
// src/sensor/MySensor.cpp
#include "MySensor.h"

ErrorCode MySensor::begin() {
    // Initialize hardware
    // Return ErrorCode::ERR_NONE on success
}

float MySensor::readDistanceMm() {
    float raw = readRawDistance();
    if (_filterEnabled) {
        return _filter.filter(raw);
    }
    return raw;
}

// ... implement all interface methods
```

### Step 3: Register in Factory

```cpp
// src/sensor/SensorFactory.cpp

ISensor* SensorFactory::createSensor(const SensorHWConfig& config) {
    String type = normalizeType(config.type);
    
    // Add new sensor type
    if (type == "MY_SENSOR") {
        return new MySensor(config.pin1, config.pin2);
    }
    
    // ... existing sensors
}
```

### Step 4: Add to Configuration

```cpp
// src/config/ConfigManager.h
// Add any new config fields

// data/config.json
// Add default configuration for new sensor
```

### Step 5: Update Filter Configuration

```cpp
// src/main.cpp - configureFilters()
else if (sensorType == "MY_SENSOR") {
    MySensor* sensor = static_cast<MySensor*>(activeSensor);
    sensor->setFilterEnabled(config.filterEnabled);
    applyFilterConfig(sensor->getFilter(), config);
}
```

---

## Build System

### PlatformIO Configuration

```ini
; platformio.ini
[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino
board_build.filesystem = littlefs

; Libraries
lib_deps = 
    ArduinoJson@^7.0.0
    PubSubClient@^2.8
    WebSockets@^2.7.0
    
; Build scripts
extra_scripts = pre:scripts/build_increment.py
```

### Build Commands

```bash
# Build firmware
pio run

# Build filesystem
pio run --target buildfs

# Upload firmware
pio run --target upload

# Upload filesystem
pio run --target uploadfs

# Clean
pio run --target clean

# Serial monitor
pio device monitor
```

### Version Management

Build number auto-increments via `scripts/build_increment.py`.
Version defined in `src/version.h`:

```cpp
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0
#define BUILD_NUMBER  14
```

---

## Code Conventions

### Naming

| Type | Convention | Example |
|------|------------|---------|
| Classes | PascalCase | `TankCalculator` |
| Functions | camelCase | `readDistanceMm()` |
| Constants | UPPER_SNAKE | `MAX_FILTER_SIZE` |
| Member vars | _prefixed | `_lastReading` |
| Parameters | camelCase | `sensorConfig` |

### File Organization

Each `.cpp` file should have sections:
```cpp
// SECTION 1: INCLUDES
// SECTION 2: CONSTRUCTOR
// SECTION 3: INITIALIZATION
// SECTION 4: CORE FUNCTIONALITY
// SECTION 5: UTILITIES
```

### Memory Management

1. Use `F()` macro for string literals (saves RAM)
2. Prefer stack allocation over heap
3. Check for null after `new`
4. Use `yield()` in long loops

### Error Handling

```cpp
// Return ErrorCode for all operations that can fail
ErrorCode result = sensor->begin();
if (result != ErrorCode::ERR_NONE) {
    ErrorHandler::getInstance().logError(result);
    // Handle error
}
```

---

## Testing

### Serial Monitor Testing

1. Enable debug in config: `debugEnabled: true`
2. Monitor output: `pio device monitor`
3. Watch for:
   - Initialization messages
   - Sensor readings
   - Error messages

### API Testing

```bash
# Get water level
curl http://192.168.1.100/api/level

# Get configuration
curl http://192.168.1.100/api/config

# Update WiFi config
curl -X POST http://192.168.1.100/api/config/wifi \
  -H "Content-Type: application/json" \
  -d '{"ssid":"MyNetwork","password":"secret"}'
```

### Memory Monitoring

Watch free heap in status output:
```
Free Heap: 32456 bytes
```

If heap drops below 10KB, investigate memory leaks.

---

## Debugging Tips

### Common Issues

1. **Sensor timeout**: Check wiring, verify sensor mode
2. **WiFi won't connect**: Check credentials, try AP mode
3. **MQTT disconnects**: Verify broker settings, check keepalive
4. **Readings unstable**: Adjust filter parameters

### Debug Output

```cpp
SystemConfig& config = ConfigManager::getInstance().getSystemConfig();
if (config.debugEnabled) {
    Serial.printf("[Module] Debug message: %d\n", value);
}
```

### Memory Dump

```cpp
Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
Serial.printf("Heap Fragmentation: %d%%\n", ESP.getHeapFragmentation());
```

---

*For API documentation, see [API Reference](API_REFERENCE.md).*

