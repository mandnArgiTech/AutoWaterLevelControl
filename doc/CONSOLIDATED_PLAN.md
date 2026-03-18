# FluidLevelMonitor × SudarshanChakra — Consolidated Integration Plan

> Replaces the earlier FEATURE_PLAN.md. This is the authoritative plan.
> Last updated: 2026-03

---

## 1. Understanding the Full System

### SudarshanChakra is already a production IoT platform

| Layer | What exists |
|-------|-------------|
| Cloud VPS (`vivasvan-tech.in`) | PostgreSQL 16, RabbitMQ 3 (MQTT plugin), 5 Spring Boot microservices, React dashboard, Nginx TLS |
| Edge AI | 2× GPU PCs running YOLOv8n, zone engine, LoRa receiver, farm_edge_node.py |
| Android app | Kotlin/Compose, HiveMQ MQTT5 foreground service, Retrofit, Hilt, Room — already ship-ready |
| Firmware | ESP32 LoRa worker tags (separate from water level) |

The AutoWaterLevelControl firmware **plugs into this as a new device class**. It does not replace any SC component — it extends the platform with water/motor awareness.

### What SudarshanChakra's WATER_LEVEL_INTEGRATION_PLAN already specified

SC already has a full plan for integrating water level data (Tasks 1–8):
- Point ESP8266 MQTT → VPS RabbitMQ broker
- New queues `water.level` and `water.status`
- PostgreSQL tables `water_tanks` and `water_level_readings`
- `WaterLevelConsumer` in alert-service
- `WaterTankController` in device-service
- React dashboard water widget
- Android water card on AlertFeedScreen
- Edge water bridge (if ESP8266 can't reach VPS directly)

**This plan must not contradict or duplicate that work.** It builds on top of it.

---

## 2. Real-World Topology — Your Actual Deployment

### Location A: Sangareddy Farm

| Item | Detail |
|------|--------|
| Tanks | 3 tanks (names TBD — e.g. farm_tank1, farm_tank2, farm_tank3) |
| Motor | 5HP, controlled by **Taro Smart Panel** |
| Motor control method | **SMS** — Taro panel accepts specific SMS messages to turn ON/OFF |
| SIM for SMS | One SIM in an ESP8266 module (GSM shield) OR dedicated GSM modem |
| Level sensors | 3× ESP8266 (one per tank) — sensor-only role, no relay |
| Network | Farm LAN → Edge Node A (10.8.0.10) → OpenVPN → VPS |

### Location B: Home

| Item | Detail |
|------|--------|
| Tanks | 2 tanks — Sump (underground) + Overhead tank |
| Motor | Standard pump, controlled by **relay** |
| Motor control method | GPIO relay on a dedicated ESP8266 module |
| Level sensors | 2× ESP8266 (one per tank) — sensor-only role |
| Relay controller | 1× ESP8266 with relay module — separate from sensor modules |
| Network | Direct WiFi → VPS MQTT (no VPN needed if home has internet) |

---

## 3. Firmware Architecture — Two Roles, One Codebase

The same `AutoWaterLevelControl` PlatformIO project builds two firmware variants using **compile-time flags**.

### Build variants

```ini
; platformio.ini

[env:sensor_only]
; Sensor (level measurement) role — default
; No relay, no GSM
; Runs on any ESP8266 with US-100/HC-SR04/TF-Luna/XKC-KD200
build_flags =
    -D FLM_ROLE_SENSOR
    -D VERSION_MAJOR=1

[env:motor_relay]
; Motor controller role — relay-based
; GPIO relay, no sensor pipeline
; Runs on ESP8266 with relay module
build_flags =
    -D FLM_ROLE_MOTOR_RELAY
    -D VERSION_MAJOR=1

[env:motor_sms]
; Motor controller role — SMS-based (Taro Smart Panel)
; GSM shield (SIM800L or A6), no relay GPIO
; Configurable ON/OFF SMS messages
build_flags =
    -D FLM_ROLE_MOTOR_SMS
    -D VERSION_MAJOR=1
```

### Per-role what gets compiled

| Module | `sensor_only` | `motor_relay` | `motor_sms` |
|--------|:---:|:---:|:---:|
| Sensor pipeline (ISensor, FilteredSensorBase, TankCalculator) | ✅ | ❌ | ❌ |
| 3-stage filter (Median→MovingAvg→Kalman) | ✅ | ❌ | ❌ |
| WebSocket calibration | ✅ | ❌ | ❌ |
| RelayManager | ❌ | ✅ | ❌ |
| SmsMotorManager | ❌ | ❌ | ✅ |
| WiFiManager + MQTT | ✅ | ✅ | ✅ |
| ConfigManager + LittleFS | ✅ | ✅ | ✅ |
| Web server / REST API | ✅ | ✅ | ✅ |
| OTA | ✅ | ✅ | ✅ |

### How roles communicate

```
[sensor_only ESP8266]                    [motor_relay / motor_sms ESP8266]
tank1_abc/water/level  ─────────────►  {deviceTag}/motor/command
tank1_abc/water/status ─────────────►  { "command": "pump_on" | "pump_off" | "pump_auto" }
                                                │
                                         Subscribed to
                                         this topic from VPS
```

The motor controller does NOT read its own sensor. It receives MQTT commands from the cloud, which receives level data from the sensor module. The cloud decides when to command the pump based on thresholds configured per-tank.

---

## 4. New Firmware Module: SmsMotorManager

### Purpose

Sends SMS messages to the Taro Smart Panel to turn the 5HP farm motor ON or OFF.

### Hardware

- ESP8266 + SIM800L (or A6) GSM shield
- SIM card with SMS capability
- Compiled with `-D FLM_ROLE_MOTOR_SMS`

### Configuration (stored in LittleFS config.json `relay` section)

```json
"relay": {
  "enabled": true,
  "controlType": "sms",
  "gsm": {
    "rxPin": 4,
    "txPin": 5,
    "baudRate": 9600,
    "targetPhone": "+91XXXXXXXXXX",
    "onMessage": "START PUMP",
    "offMessage": "STOP PUMP",
    "confirmationTimeoutMs": 30000
  },
  "pumpOnPercent": 20.0,
  "pumpOffPercent": 85.0,
  "maxRunMinutes": 60
}
```

The `onMessage` and `offMessage` are **fully configurable** — different Taro panels may need different command strings. Users configure these via the app or web UI.

### REST endpoints (same as relay)

```
GET  /api/pump   → { "state": "stopped"|"running", "mode": "auto"|"on"|"off", "controlType": "sms" }
POST /api/pump   → { "state": "on"|"off"|"auto" }
```

### MQTT (same topics as relay)

```
{deviceTag}/motor/command  →  subscribe  { "command": "pump_on"|"pump_off"|"pump_auto" }
{deviceTag}/motor/status   →  publish    { "state": "...", "mode": "...", "runSeconds": N }
```

---

## 5. Revised Device Model

Each physical location has multiple ESP8266 nodes. SudarshanChakra registers each as a device.

### Sangareddy Farm

| Node ID | Role | Sensor | MQTT publishes |
|---------|------|--------|----------------|
| `farm_tank1_<chipid>` | sensor_only | US-100 | `farm_tank1_xxx/water/level` |
| `farm_tank2_<chipid>` | sensor_only | US-100 | `farm_tank2_xxx/water/level` |
| `farm_tank3_<chipid>` | sensor_only | US-100 | `farm_tank3_xxx/water/level` |
| `farm_motor_<chipid>` | motor_sms | none | subscribes to `farm_motor_xxx/motor/command` |

### Home

| Node ID | Role | Sensor | MQTT publishes |
|---------|------|--------|----------------|
| `home_sump_<chipid>` | sensor_only | US-100/HC-SR04 | `home_sump_xxx/water/level` |
| `home_overhead_<chipid>` | sensor_only | US-100 | `home_overhead_xxx/water/level` |
| `home_motor_<chipid>` | motor_relay | none | subscribes to `home_motor_xxx/motor/command` |

---

## 6. SudarshanChakra Backend — What Needs Adding

The SC backend already handles alerts, devices, and cameras. Water level needs to extend it.

### 6.1 New MQTT topic namespace for motor commands

```
SC cloud publishes:
  {motorDeviceTag}/motor/command   { "command": "pump_on" | "pump_off" | "pump_auto" }

Motor ESP8266 publishes back:
  {motorDeviceTag}/motor/status    { "state": "running"|"stopped", "runSeconds": N, "controlType": "relay"|"sms" }
  {motorDeviceTag}/motor/alert     { "type": "max_runtime"|"dry_run_blocked"|"sms_failed", "message": "..." }
```

New RabbitMQ queues:
```
water.level       — existing (from SC plan)
water.status      — existing (from SC plan)
motor.status      — new: motor controller heartbeat
motor.alert       — new: motor safety events
```

### 6.2 New PostgreSQL table: `water_tank_groups`

The SC plan has `water_tanks` (one row per tank). We need to add the concept of a **motor** that serves one or more tanks.

```sql
-- Links tanks to motor controllers
-- A motor can serve 1-N tanks (e.g., one 5HP pump fills all 3 farm tanks)
CREATE TABLE water_motor_controllers (
    id VARCHAR(50) PRIMARY KEY,           -- e.g., "farm_motor", "home_motor"
    farm_id UUID NOT NULL,
    display_name VARCHAR(100),
    device_tag VARCHAR(100),              -- e.g., "farm_motor_a9b1c2"
    control_type VARCHAR(10) NOT NULL     -- "relay" or "sms"
        CHECK (control_type IN ('relay', 'sms')),
    gsm_target_phone VARCHAR(20),         -- for SMS type
    gsm_on_message VARCHAR(100),          -- configurable Taro panel ON command
    gsm_off_message VARCHAR(100),         -- configurable Taro panel OFF command
    auto_mode BOOLEAN DEFAULT TRUE,
    pump_on_percent REAL DEFAULT 20.0,    -- trigger level across served tanks
    pump_off_percent REAL DEFAULT 85.0,
    max_run_minutes INTEGER DEFAULT 30,
    status VARCHAR(20) DEFAULT 'unknown',
    last_seen_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Links which tanks a motor serves
CREATE TABLE water_tank_motor_map (
    tank_id VARCHAR(50) REFERENCES water_tanks(id) ON DELETE CASCADE,
    motor_id VARCHAR(50) REFERENCES water_motor_controllers(id) ON DELETE CASCADE,
    is_primary BOOLEAN DEFAULT TRUE,     -- primary fill tank for this motor
    PRIMARY KEY (tank_id, motor_id)
);
```

### 6.3 New REST endpoints in device-service

```
GET  /api/v1/water/motors             → List all motor controllers
GET  /api/v1/water/motors/{id}        → Motor detail + linked tanks + status
POST /api/v1/water/motors/{id}/command → { "command": "pump_on"|"pump_off"|"pump_auto" }
PUT  /api/v1/water/motors/{id}        → Update thresholds, SMS config
GET  /api/v1/water/motors/{id}/history → Motor run history (start/stop events)
```

### 6.4 Motor auto-command logic in device-service

When a water level reading arrives from a tank:

1. Look up which motor serves that tank (`water_tank_motor_map`)
2. Check that motor's auto_mode + thresholds
3. If level < `pump_on_percent` and motor is stopped → publish `pump_on` to `{motorDeviceTag}/motor/command`
4. If level >= `pump_off_percent` and motor is running → publish `pump_off` to `{motorDeviceTag}/motor/command`
5. Never command motor if another linked tank is already being filled (prevents overcurrent)

This logic lives in the cloud, not the motor ESP8266, because the cloud has visibility across all tanks.

---

## 7. Android App — Revised Plan

### Key constraint: The water screens must be INSIDE the SudarshanChakra Android app

The AutoWaterLevelControl Android app we started building is **wrong** — it should not be a standalone app. Water monitoring and motor control become **new screens inside the SudarshanChakra app**.

The SC app already has:
- Login (JWT auth via SC backend)
- Alert feed (farm/alerts/# MQTT subscription)
- Camera grid
- Siren control
- Device status

**We add these screens to the SC app:**
- `WaterTanksScreen` — all tanks grouped by location, with level gauges
- `WaterTankDetailScreen` — single tank: live level, 24h chart, linked motor
- `MotorControlScreen` — control pump (relay OR SMS), mode, thresholds
- `MotorSmsConfigScreen` — configure Taro panel phone number and message strings

### Revised SC Android NavGraph additions

```kotlin
const val WATER_TANKS    = "water_tanks"
const val WATER_DETAIL   = "water_detail/{tankId}"
const val MOTOR_CONTROL  = "motor_control/{motorId}"
const val MOTOR_SMS_CFG  = "motor_sms_config/{motorId}"
```

### Water card on AlertFeedScreen

Already planned in SC WATER_LEVEL_INTEGRATION_PLAN (Task 7). Implement as a summary strip at top of feed:

```
┌─────────────────────────────────────────────────────────┐
│  💧 Farm: T1 72%  T2 45%  T3 89%  |  Home: S 61% O 34%│
│  Motor: Farm STOPPED  •  Home RUNNING (12min)           │
└─────────────────────────────────────────────────────────┘
```

Tapping opens WaterTanksScreen.

### Motor control flow from Android

```
User taps "Start Pump"
  → POST /api/v1/water/motors/{id}/command { "command": "pump_on" }
  → SC device-service publishes MQTT to {motorDeviceTag}/motor/command
  → Motor ESP8266 receives command
  → Relay closes (motor_relay) OR SMS sent (motor_sms)
  → Motor publishes status back to {motorDeviceTag}/motor/status
  → SC device-service receives, updates DB, pushes via WebSocket
  → Android WaterSocket subscription updates UI
```

---

## 8. Revised Firmware Roadmap

### Phase 1 — Sensor stability (DONE in current branch)
- [x] ISensor interface + FilteredSensorBase
- [x] 3-stage filter pipeline
- [x] ConfigManager + LittleFS
- [x] WiFi + MQTT + OTA
- [x] REST API + Web UI
- [x] Bug fixes: yield(), watchdog, MQTT backoff

### Phase 2 — Firmware role split (NEXT)

**New files needed:**

```
src/
  motor/
    IMotorController.h        — Interface: begin(), commandOn(), commandOff(), getStatusJson()
    RelayMotorController.h/cpp — GPIO relay implementation (was RelayManager)
    SmsMotorController.h/cpp   — SIM800L/A6 SMS implementation (Taro Smart Panel)
    MotorControllerFactory.h/cpp — creates correct impl from config
  main_sensor.cpp             — setup()/loop() for FLM_ROLE_SENSOR
  main_motor.cpp              — setup()/loop() for FLM_ROLE_MOTOR_*
```

Rename current `RelayManager` → `RelayMotorController` implementing `IMotorController`.

**platformio.ini** gets 3 environments as described in Section 3.

**Config additions for SMS:**

```json
"relay": {
  "enabled": true,
  "controlType": "relay",     // or "sms"
  "pin": 12,                  // relay only
  "activeLow": true,          // relay only
  "gsm": {                    // sms only
    "rxPin": 4,
    "txPin": 5,
    "targetPhone": "+91XXXXXXXXXX",
    "onMessage": "START PUMP",
    "offMessage": "STOP PUMP"
  },
  "pumpOnPercent": 20.0,
  "pumpOffPercent": 85.0,
  "maxRunMinutes": 60
}
```

### Phase 3 — SudarshanChakra integration

- Motor controller ESP8266 subscribes to `{motorDeviceTag}/motor/command` 
- SC device-service implements auto-command logic (pump on/off based on tank levels)
- SC backend registers motor controllers + tank-motor mapping
- SC Android app gets WaterTanksScreen + MotorControlScreen

### Phase 4 — Alerts & history

- SC alert-service already has threshold-based alert generation (from existing plan)
- Add motor events: `max_runtime`, `dry_run_blocked`, `sms_failed`, `sms_confirmed`
- These flow into the standard SC alert pipeline → dashboard → Android push notifications
- LittleFS ring-buffer history on sensor nodes (24h, 5-min intervals) for offline resilience

---

## 9. What to Delete / Not Build

The standalone `AutoWaterLevelControl/android/` directory we built is the wrong approach. It should be **discarded** and the water screens built inside `SudarshanChakra/android/` instead.

The `RelayManager` we built is the right logic but needs to be refactored as `RelayMotorController` implementing `IMotorController`, so the SMS variant can be added cleanly.

---

## 10. Immediate Next Actions

### Firmware (AutoWaterLevelControl branch)
1. Refactor `RelayManager` → `IMotorController` + `RelayMotorController`
2. Create `SmsMotorController` (SIM800L AT commands for SMS)
3. Create `MotorControllerFactory`
4. Split `main.cpp` into role-based compile variants
5. Add `controlType` and `gsm` fields to `RelayConfig` / ConfigManager
6. Update platformio.ini with 3 environments

### Backend (SudarshanChakra)
1. Implement Tasks 1–8 from WATER_LEVEL_INTEGRATION_PLAN.md (already specced)
2. Add `water_motor_controllers` and `water_tank_motor_map` tables
3. Add motor REST endpoints to device-service
4. Add motor auto-command logic in device-service WaterLevelConsumer
5. Add `motor.status` and `motor.alert` RabbitMQ queues

### Android (SudarshanChakra app — not standalone)
1. Add `WaterTank` and `MotorController` domain models
2. Add water/motor endpoints to `ApiService.kt`
3. Add water MQTT topics to `MqttForegroundService`
4. Build `WaterTanksScreen`, `WaterTankDetailScreen`, `MotorControlScreen`, `MotorSmsConfigScreen`
5. Add water strip card to `AlertFeedScreen`
6. Wire into existing `NavGraph.kt`

