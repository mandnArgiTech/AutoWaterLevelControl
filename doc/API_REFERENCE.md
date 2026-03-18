# FluidLevelMonitor - API Reference

## Table of Contents
1. [REST API](#rest-api)
2. [MQTT Interface](#mqtt-interface)
3. [WebSocket API](#websocket-api)
4. [Data Structures](#data-structures)

---

## REST API

Base URL: `http://<device-ip>/api`

### Authentication

Currently no authentication required. Device should be on trusted network.

---

### Status Endpoints

#### GET /api/status

Returns complete system status.

**Response:**
```json
{
  "device": {
    "name": "Water Tank Monitor",
    "version": "1.0.0",
    "build": 14,
    "uptime": "2d 5h 30m",
    "freeHeap": 32456,
    "chipId": "ABC123"
  },
  "wifi": {
    "connected": true,
    "ssid": "HomeNetwork",
    "ip": "192.168.1.100",
    "rssi": -65,
    "quality": 70
  },
  "mqtt": {
    "enabled": true,
    "connected": true,
    "server": "192.168.1.50"
  },
  "level": {
    "percentFilled": 75.2,
    "percentRemaining": 24.8,
    "waterHeightCm": 128.0,
    "volumeLiters": 1832.5,
    "state": "NORMAL",
    "valid": true
  },
  "sensor": {
    "type": "US-100",
    "distanceMm": 425.5,
    "temperature": 28.5
  }
}
```

---

#### GET /api/level

Returns current water level data.

**Response:**
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

**Level States:**
| State | Description |
|-------|-------------|
| `EMPTY` | Tank is empty (< 5%) |
| `LEVEL_LOW` | Low level warning (< 20%) |
| `NORMAL` | Normal operating range |
| `HIGH` | High level (> 90%) |
| `FULL` | Tank is full (> 98%) |
| `OVERFLOW` | Overflow condition |
| `ERROR` | Sensor error |

---

#### GET /api/sensor

Returns sensor status and diagnostics.

**Response:**
```json
{
  "type": "US-100",
  "initialized": true,
  "rxPin": 5,
  "txPin": 4,
  "temperature": 28.5,
  "calibrationOffset": 50.0,
  "calibrationOffsetCm": 5.0,
  "minRange": 20,
  "maxRange": 4500,
  "readCount": 15432,
  "errorCount": 12,
  "errorRate": 0.08,
  "filter": {
    "enabled": true,
    "ready": true,
    "medianSize": 5,
    "movingAvgWindow": 10,
    "kalmanEnabled": true,
    "kalmanGain": 0.45
  },
  "lastReading": {
    "distanceMm": 425.5,
    "distanceCm": 42.55,
    "valid": true,
    "timestamp": 1705300200000
  }
}
```

---

#### GET /api/info

Returns device information.

**Response:**
```json
{
  "device": "FluidLevelMonitor",
  "version": "1.0.0",
  "build": 14,
  "buildDate": "2024-01-15",
  "chipId": "ABC123",
  "flashSize": 4194304,
  "freeHeap": 32456,
  "sdkVersion": "3.0.0"
}
```

---

### Configuration Endpoints

#### GET /api/config

Returns complete configuration.

**Response:**
```json
{
  "wifi": {
    "ssid": "HomeNetwork",
    "password": "********",
    "hostname": "FluidMonitor",
    "apSsid": "FluidMonitor-AP",
    "apPassword": "12345678",
    "connectTimeout": 30000,
    "apMode": false
  },
  "mqtt": {
    "enabled": true,
    "server": "192.168.1.50",
    "port": 1883,
    "username": "",
    "password": "",
    "clientId": "FluidMonitor",
    "topicPrefix": "home/water",
    "publishInterval": 60000
  },
  "tank": {
    "type": "circular",
    "diameter": 1350.0,
    "height": 1704.5,
    "volumeLiters": 2438.0
  },
  "sensor": {
    "hardware": {
      "type": "US100",
      "rxPin": 5,
      "txPin": 4
    },
    "offsetMm": 50.0,
    "filterEnabled": true,
    "medianFilterSize": 5,
    "movingAvgWindow": 10,
    "kalmanEnabled": true,
    "kalmanProcessNoise": 0.01,
    "kalmanMeasureNoise": 0.1
  },
  "system": {
    "deviceName": "Water Tank Monitor",
    "timezoneOffset": 19800,
    "ntpServer": "pool.ntp.org",
    "debugEnabled": true
  }
}
```

---

#### POST /api/config

Update complete configuration.

**Request:**
```json
{
  "wifi": { ... },
  "mqtt": { ... },
  "tank": { ... },
  "sensor": { ... },
  "system": { ... }
}
```

**Response:**
```json
{
  "success": true,
  "message": "Configuration saved"
}
```

---

#### GET /api/config/{section}

Get specific configuration section.

**Sections:** `wifi`, `mqtt`, `tank`, `sensor`, `system`

**Example:** `GET /api/config/tank`

**Response:**
```json
{
  "type": "circular",
  "diameter": 1350.0,
  "height": 1704.5,
  "volumeLiters": 2438.0
}
```

---

#### POST /api/config/{section}

Update specific configuration section.

**Example:** `POST /api/config/tank`

**Request:**
```json
{
  "type": "circular",
  "diameter": 1100.0,
  "height": 1320.0
}
```

**Response:**
```json
{
  "success": true,
  "message": "Tank configuration saved"
}
```

---

### WiFi Endpoints

#### GET /api/wifi/status

Returns WiFi status.

**Response:**
```json
{
  "mode": "STATION",
  "connected": true,
  "ssid": "HomeNetwork",
  "ip": "192.168.1.100",
  "mac": "AA:BB:CC:DD:EE:FF",
  "rssi": -65,
  "quality": 70,
  "hostname": "FluidMonitor"
}
```

---

#### GET /api/wifi/scan

Scan for available networks.

**Response:**
```json
{
  "networks": [
    {
      "ssid": "HomeNetwork",
      "rssi": -65,
      "encrypted": true,
      "channel": 6
    },
    {
      "ssid": "Neighbor_WiFi",
      "rssi": -80,
      "encrypted": true,
      "channel": 11
    }
  ]
}
```

---

### System Endpoints

#### POST /api/restart

Restart the device.

**Response:**
```json
{
  "success": true,
  "message": "Restarting..."
}
```

---

#### POST /api/reset

Factory reset - clear all configuration.

**Response:**
```json
{
  "success": true,
  "message": "Factory reset complete"
}
```

---

#### GET /api/errors

Get error log.

**Response:**
```json
{
  "errors": [
    {
      "code": 101,
      "name": "ERR_SENSOR_TIMEOUT",
      "description": "Sensor response timeout",
      "count": 3,
      "lastOccurrence": "2024-01-15T10:25:00+05:30"
    }
  ]
}
```

---

#### POST /api/errors/clear

Clear error log.

**Response:**
```json
{
  "success": true,
  "message": "Error log cleared"
}
```

---

## MQTT Interface

### Connection

| Setting | Default |
|---------|---------|
| Port | 1883 |
| Keep Alive | 60 seconds |
| QoS | 0 |
| Retain | false |

### Published Topics

#### {prefix}/level

Published at configured interval (default: 60 seconds).

**Payload:**
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

---

#### {prefix}/status

Published on connect and periodically.

**Payload:**
```json
{
  "online": true,
  "version": "1.0.0",
  "ip": "192.168.1.100",
  "rssi": -65,
  "uptime": "2d 5h 30m",
  "freeHeap": 32456
}
```

---

### Subscribed Topics

#### {prefix}/command

Device listens for commands.

**Commands:**

| Command | Description |
|---------|-------------|
| `read` | Force sensor read and publish |
| `status` | Publish device status |
| `restart` | Restart device |

**Example:**
```json
{
  "command": "read"
}
```

---

### Last Will Testament

**Topic:** `{prefix}/status`
**Payload:** `{"online": false}`

---

## WebSocket API

### Calibration WebSocket

**URL:** `ws://<device-ip>:81/`

Used for real-time sensor calibration.

### Messages from Server

#### Distance Reading
```json
{
  "type": "reading",
  "distanceMm": 425.5,
  "distanceCm": 42.55,
  "raw": 430.2,
  "filtered": 425.5,
  "valid": true,
  "timestamp": 1705300200000
}
```

#### Calibration Result
```json
{
  "type": "calibration",
  "success": true,
  "offset": 50.0,
  "message": "Calibration saved"
}
```

### Messages to Server

#### Request Calibration
```json
{
  "type": "calibrate",
  "knownDistanceCm": 100.0,
  "measuredDistanceCm": 95.5
}
```

#### Request Reading
```json
{
  "type": "read"
}
```

---

## Data Structures

### WaterLevel

| Field | Type | Description |
|-------|------|-------------|
| `percentFilled` | float | Percentage of tank filled (0-100) |
| `percentRemaining` | float | Percentage remaining to fill (0-100) |
| `waterHeightCm` | float | Water height in centimeters |
| `distanceCm` | float | Distance from sensor to water (cm) |
| `volumeLiters` | float | Current water volume in liters |
| `volumeRemaining` | float | Remaining capacity in liters |
| `state` | string | Tank state enum |
| `valid` | bool | Reading validity |

### SensorReading

| Field | Type | Description |
|-------|------|-------------|
| `distanceMm` | float | Distance in millimeters |
| `distanceCm` | float | Distance in centimeters |
| `temperatureC` | float | Temperature (if supported) |
| `distanceValid` | bool | Distance reading validity |
| `temperatureValid` | bool | Temperature reading validity |
| `timestamp` | ulong | Reading timestamp (millis) |
| `lastError` | int | Error code if any |

### ErrorCode

| Code | Name | Description |
|------|------|-------------|
| 0 | ERR_NONE | No error |
| 100 | ERR_SENSOR_INIT | Sensor initialization failed |
| 101 | ERR_SENSOR_TIMEOUT | Sensor timeout |
| 102 | ERR_SENSOR_INVALID_DATA | Invalid sensor data |
| 103 | ERR_SENSOR_DISTANCE_MIN | Distance below minimum |
| 104 | ERR_SENSOR_DISTANCE_MAX | Distance above maximum |
| 200 | ERR_WIFI_NO_SSID | No SSID configured |
| 201 | ERR_WIFI_CONNECT | Connection failed |
| 300 | ERR_MQTT_CONNECT | MQTT connection failed |
| 400 | ERR_CONFIG_LOAD | Config load error |
| 401 | ERR_CONFIG_SAVE | Config save error |

---

## HTTP Status Codes

| Code | Meaning |
|------|---------|
| 200 | Success |
| 400 | Bad request (invalid JSON) |
| 404 | Endpoint not found |
| 500 | Internal server error |

---

## Rate Limiting

No rate limiting implemented. Recommended polling intervals:
- Status: 5+ seconds
- Level: 1+ second
- Config: As needed

---

*For integration examples, see the [User Guide](USER_GUIDE.md).*

