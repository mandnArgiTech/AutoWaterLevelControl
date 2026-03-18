# Manual test matrix (FluidLevelMonitor)

Run after firmware or doc-affecting changes. Check serial @ 115200 for `[FLM][LEVEL][Module]` lines.

| # | Scenario | Pass criteria |
|---|----------|----------------|
| 1 | Cold boot (STA configured) | WiFi connects, web UI loads, `/api/status` 200 |
| 2 | AP mode (clear/wrong SSID) | AP up, `192.168.4.1` serves UI or API |
| 3 | GET /api/status | Matches schema (heap, level, connection) |
| 4 | POST /api/config section | 200; device behavior reflects change |
| 5 | POST /api/config full + oversize body | 413 `PAYLOAD_TOO_LARGE` if >12k |
| 6 | Concurrent config | Second POST during first returns 503 `CONFIG_LOCKED` |
| 7 | Web OTA /api/update | Success reboots; abort restores MQTT/WS |
| 8 | Arduino OTA | Upload via IDE; fail path restores HTTP |
| 9 | MQTT publish | Broker receives `level` on interval |
| 10 | Calibration WS | `saveCalibration` persists; `CONFIG_LOCKED` if HTTP save active |
| 11 | POST /api/restart | Device reboots |
| 12 | Long run 1h+ | Heap stable, no watchdog reset |

## Static analysis

```bash
pio check -e nodemcuv2
```

`pio check` may report ArduinoJson preprocessor noise in `libdeps` — ignore for vendor code. Project sources should stay clean.
