# FluidLevelMonitor — Feature Plan & Roadmap

> Branch: `feature/fluid-level-monitor-esp8266`  
> Integration target: **SudarshanChakra** platform  
> Last updated: 2026-03

---

## 1. Code Review Summary

### ✅ Strengths

| Area | Assessment |
|------|-----------|
| Sensor abstraction (`ISensor` + `FilteredSensorBase`) | Excellent — adding a new sensor requires implementing only 4 methods |
| 3-stage filter pipeline (Median → Moving Avg → Kalman) | Correct approach for fluid surface measurement; all params runtime-configurable |
| Atomic config save (temp file + backup) | Production-grade; prevents corruption on power loss |
| OTA lifecycle coordination (MQTT + WS + HTTP) | Non-trivial and correctly handled |
| Modular architecture (sensor / network / tank / utils) | Clean separation of concerns |
| REST API completeness | Well-designed; covers status, config, OTA, errors |
| Error handler with history + JSON lookups | Thoughtful design with cache |

### ⚠️ Issues Found

| # | Severity | Issue | File | Fix |
|---|----------|-------|------|-----|
| 1 | 🔴 CRITICAL | Real WiFi/MQTT credentials committed to `data/config.json` | `data/config.json` | Sanitized in this commit; add to `.gitignore` secret pattern |
| 2 | 🔴 HIGH | `readDistanceAverageMm()` blocks with `delay()` — drops WebSocket frames during calibration | `FilteredSensorBase.cpp` | Replace `delay()` with `yield()`; or make non-blocking |
| 3 | 🟠 MEDIUM | Single `volatile bool` config lock — silent drop on conflict | `ConfigManager.h` | Add `_savePending` flag; flush in `processLoop()` |
| 4 | 🟠 MEDIUM | No hardware watchdog — hang on blocked SPIFFS read or TCP half-open | `main.cpp` | Add `ESP.wdtEnable(8000)` + `ESP.wdtFeed()` in loop |
| 5 | 🟡 LOW | MQTT reconnect max backoff 120s too long for pump-control use case | `MQTTManager.h` | Cap at `30000` ms |
| 6 | 🟡 LOW | `WaterLevel.timestamp` uses `millis()` but MQTT payload uses ISO8601 — can diverge | `TankCalculator.cpp` | Stamp ISO time at `calculate()` time, not publish time |
| 7 | 🟡 LOW | `errors.json` SPIFFS open per cache miss (~50–80 ms) — stalls loop on error cascade | `ErrorHandler.cpp` | Load into RAM `JsonDocument` at boot; fallback static table |
| 8 | 🟡 LOW | Web OTA has no concurrent upload guard | `WebServerFirmware.cpp` | Add `_uploadInProgress` flag |

---

## 2. Immediate Missing Feature: Android App

**Priority: P0**

The device has a complete REST API and MQTT bus, but no mobile interface. For field use (checking tank level from a phone, controlling the pump from anywhere on the same network), a native Android app is the highest-impact addition.

### What the app must do

| Feature | Details |
|---------|---------|
| **Live water level** | Animated tank graphic + percentage + volume in litres |
| **Pump control** | ON / OFF / AUTO toggle; safety interlock (won't run pump if level < 5%) |
| **Alerts** | Push notification when level < configurable low threshold or > 95% |
| **Device discovery** | mDNS / manual IP entry to connect to `FluidMonitor.local` |
| **Settings** | Configure device IP, alert thresholds, auto-pump thresholds |
| **Sensor status** | Show sensor type, last reading, error history |

### Communication design (SudarshanChakra integration)

The app will connect to the device via two channels:

```
Android App
├── REST polling (HTTP)     →  GET /api/status  (every 5 s on foreground)
├── REST polling (HTTP)     →  GET /api/level    (on-demand)
├── MQTT subscribe          →  {deviceTag}/water/level  (real-time)
└── MQTT publish            →  {deviceTag}/water/command  { "command": "pump_on" | "pump_off" }
```

When the **SudarshanChakra** backend is available, the app will authenticate through it instead of connecting directly, and all MQTT goes through SudarshanChakra's broker.

---

## 3. Full Feature Roadmap

### Phase 1 — Core stability (current sprint)

- [x] 3-stage sensor filter pipeline
- [x] REST API + Web UI
- [x] OTA update support
- [x] MQTT publish
- [ ] **Fix: sanitize credentials in config.json** ← done in this commit
- [ ] **Fix: replace `delay()` with `yield()` in averaging loop**
- [ ] **Fix: watchdog timer**
- [ ] **Fix: MQTT reconnect cap to 30 s**
- [ ] **Android App — Phase 1 (status view + pump control)**

### Phase 2 — Pump relay control (next sprint)

Add `RelayManager` module to the firmware:

```
src/relay/
  RelayManager.h
  RelayManager.cpp
```

| Feature | Detail |
|---------|--------|
| GPIO control | Configurable pin, active-low/high |
| AUTO mode | Turn ON when level < `pumpOnPercent`, OFF when level > `pumpOffPercent` |
| Manual override | Via MQTT command and REST `POST /api/pump` |
| Dry-run guard | Never run pump if `percentFilled < 5` |
| Max runtime cutoff | Safety timer — auto-off after configurable minutes |
| State publishing | MQTT `{deviceTag}/water/pump` with `{ "state": "on" | "off" | "auto", "runSeconds": N }` |

New MQTT commands:
```json
{ "command": "pump_on" }
{ "command": "pump_off" }  
{ "command": "pump_auto" }
```

New REST endpoints:
```
POST /api/pump          body: { "state": "on" | "off" | "auto" }
GET  /api/pump          → { "state": "...", "runSeconds": N, "autoEnabled": bool }
```

New config section:
```json
"relay": {
  "enabled": true,
  "pin": 12,
  "activeLow": true,
  "pumpOnPercent": 20,
  "pumpOffPercent": 80,
  "maxRunMinutes": 30,
  "autoMode": true
}
```

### Phase 3 — Alerts & history (sprint +2)

| Feature | Detail |
|---------|--------|
| Alert thresholds | Low (default 20%), High (default 95%), configurable |
| Alert cooldown | Don't re-alert for same condition within N minutes |
| MQTT alert topic | `{deviceTag}/water/alert` with `{ "type": "low" | "high" | "pump_timeout", "value": N }` |
| History ring buffer | `/log.csv` on LittleFS — last 24 h at 5-min intervals |
| REST history API | `GET /api/history?hours=24` |
| Android push notifications | Via FCM when MQTT alert received (requires SudarshanChakra backend relay) |

### Phase 4 — SudarshanChakra integration (sprint +3)

| Feature | Detail |
|---------|--------|
| Auth | Device registers with SudarshanChakra on boot; JWT in MQTT username |
| Topic namespace | `sc/{orgId}/{deviceId}/water/...` |
| Multi-device dashboard | SudarshanChakra aggregates multiple FluidMonitor devices |
| Remote access | App connects to SudarshanChakra cloud broker (not direct device IP) |
| Config sync | SudarshanChakra can push config updates to device |

---

## 4. Android App — Technical Specification

### Tech stack

| Layer | Choice | Reason |
|-------|--------|--------|
| Language | Kotlin | Modern, concise, coroutine support |
| UI framework | Jetpack Compose | Declarative UI, easy to maintain |
| HTTP | Retrofit + OkHttp | REST API calls |
| MQTT | HiveMQ MQTT client for Android | Official, well-maintained |
| DI | Hilt | Standard for Android |
| Architecture | MVVM + Repository | Testable, lifecycle-aware |
| Notifications | FCM | Push alerts |

### Project structure

```
app/
└── src/main/
    ├── java/com/mandnargitech/fluidlevelmonitor/
    │   ├── data/
    │   │   ├── api/          FluidApi.kt (Retrofit interface)
    │   │   ├── mqtt/         MqttRepository.kt
    │   │   ├── model/        WaterLevel.kt, PumpState.kt, DeviceStatus.kt
    │   │   └── repository/   DeviceRepository.kt
    │   ├── ui/
    │   │   ├── dashboard/    DashboardScreen.kt, DashboardViewModel.kt
    │   │   ├── pump/         PumpControlScreen.kt
    │   │   ├── settings/     SettingsScreen.kt
    │   │   └── theme/        Theme.kt, Color.kt
    │   └── di/               AppModule.kt
    └── res/
        └── values/           strings.xml, colors.xml
```

### API integration

The app polls `GET /api/status` every 5 seconds when in the foreground. On MQTT connect, it subscribes to `{deviceTag}/water/#` for real-time updates (overrides polling).

Pump commands go to `POST /api/pump` (Phase 2 firmware) with JSON body, or via MQTT publish to `{deviceTag}/water/command`.

---

## 5. Security Checklist

- [ ] Remove real credentials from `data/config.json` ← **done this commit**
- [ ] Add `data/config.json` to `.gitignore` (or add a `config.json.example`)
- [ ] Generate random AP password from chip ID on first boot (no default `12345678`)  
- [ ] Add HTTP basic auth option to REST API (`system.webPassword` config field)
- [ ] Document "device must be on trusted LAN" requirement prominently in README
- [ ] Consider TLS for MQTT (already partially referenced in main branch)

---

## 6. Definition of Done

A feature is considered complete when:
1. Firmware compiles without warnings (`pio run`)
2. Unit tests pass (`pio test`)
3. Manual test matrix updated (`MANUAL_TEST_MATRIX.md`)
4. API reference updated if endpoints changed (`API_REFERENCE.md`)
5. Android app updated to consume new endpoints/topics

