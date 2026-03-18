# FluidLevelMonitor — Android App

Kotlin + Jetpack Compose companion app for the FluidLevelMonitor ESP8266 firmware.

## Features

- **Live dashboard** — animated tank level graphic, percentage, volume in litres
- **Pump control** — ON / OFF / AUTO modes with configurable thresholds (Phase 2 firmware)
- **Sensor status** — filter pipeline visualization, connectivity health, system info
- **Alerts** — low/high level notifications via MQTT
- **SudarshanChakra ready** — settings screen has SC broker + auth token fields

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Language | Kotlin 1.9 |
| UI | Jetpack Compose + Material 3 |
| Architecture | MVVM + Repository |
| DI | Hilt |
| HTTP | Retrofit 2 + OkHttp |
| MQTT | HiveMQ MQTT Client for Android |
| State | Kotlin Flow + StateFlow |
| Settings | DataStore Preferences |

## Communication

```
App ──► GET /api/status    (poll every 5 s, foreground)
App ──► GET /api/level     (on-demand refresh)
App ──► POST /api/pump     (pump control — Phase 2 firmware)
App ──► POST /api/restart  (device restart)

MQTT subscribe  {deviceTag}/water/#      (real-time level + alerts)
MQTT publish    {deviceTag}/water/command { "command": "pump_on" | "pump_off" | "pump_auto" }
```

## Project structure

```
app/src/main/java/com/mandnargitech/fluidlevelmonitor/
├── data/
│   ├── api/               FluidApi.kt (Retrofit)
│   ├── model/             WaterLevel.kt, DeviceStatus.kt, PumpState.kt
│   ├── mqtt/              MqttRepository.kt (HiveMQ)
│   └── repository/        DeviceRepository.kt
├── ui/
│   ├── dashboard/         DashboardViewModel.kt
│   ├── pump/              PumpViewModel.kt
│   ├── sensor/            (SensorScreen.kt — to be implemented)
│   ├── settings/          (SettingsScreen.kt — to be implemented)
│   └── theme/             (Theme.kt — to be implemented)
└── di/                    AppModule.kt (Hilt)
```

## Mockup

See `doc/mockups/android_app_mockup.html` — open in any browser for an interactive preview of all four screens.

## Build

```bash
cd android
./gradlew assembleDebug
```

Requires Android Studio Hedgehog or later, JDK 17.

## SudarshanChakra integration (Phase 4)

When SudarshanChakra backend is available:
1. The app authenticates against SC's auth endpoint
2. MQTT connection switches from direct device broker → SC cloud broker
3. Topic namespace changes to `sc/{orgId}/{deviceId}/water/...`
4. The base URL for REST calls routes through SC's API gateway

No app code changes are needed for Phase 1–3; the Settings screen
`SC Broker URL` and `Auth token` fields activate Phase 4 routing.
