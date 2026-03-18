# FluidLevelMonitor — API Reference

Base URL: `http://<device-ip>/` (STA) or `http://192.168.4.1/` (AP). WebSocket calibration: `ws://<ip>:81/`.

**Auth:** None (trusted LAN). **Config POST body limit:** 12288 bytes (`FLM_MAX_CONFIG_JSON_BYTES`).

---

## REST — Status

### GET /api/status

Aggregated snapshot.

| Field | Type | Description |
|-------|------|-------------|
| `device` | string | `system.deviceName` |
| `firmware` | string | e.g. `1.0.0+42` |
| `uptime` | string | Human-readable uptime |
| `freeHeap` | number | Bytes |
| `heapFragmentation` | number | % (ESP8266) |
| `maxFreeBlock` | number | Largest free heap block |
| `timestamp` | string | ISO8601 (NTP if synced) |
| `sensorOk` | bool | Last reading valid |
| `sensorType` | string | e.g. US100 |
| `level` | object | If `sensorOk`: `percentFilled`, `percentRemaining`, `waterHeightCm`, `volumeLiters`, `volumeRemaining`, `state`. Else: `state`=`sensor_error`, `errorCode`, `errorDescription` |
| `connection` | object | `wifi`, `mqtt` (bool), `ip` |

### GET /api/level

Fresh calculation JSON (tank state, volumes, timestamps). Same shape as MQTT `level` payload fields where applicable.

### GET /api/sensor

Sensor driver status JSON (`getStatusJson()`).

### GET /api/info

Build metadata: `project`, `version`, `build`, `chipId`, `flashSize`, `sdkVersion`, etc.

---

## REST — Configuration

### GET /api/config

Full config JSON (includes WiFi/MQTT passwords — do not expose publicly).

### POST /api/config

Full replace. Body: JSON with optional sections `wifi`, `mqtt`, `tank`, `sensor`, `system`.

**Success:** `{ "success": true, "message": "..." }`

**Failures (examples):**

| HTTP | `code` | When |
|------|--------|------|
| 400 | `NO_BODY` | Missing body |
| 400 | `CONFIG_PARSE` | Invalid JSON |
| 400 | `CONFIG_VALIDATE` | Validation failed |
| 413 | `PAYLOAD_TOO_LARGE` | Body > 12288 B |
| 503 | `CONFIG_LOCKED` | Concurrent write |
| 507 | `FS_FULL` | LittleFS full |
| 500 | `CONFIG_SAVE` | Atomic save failed |

### GET/POST /api/config/{section}

`section` ∈ `wifi` | `mqtt` | `tank` | `sensor` | `system`. POST body is JSON for that section only. Same error schema as above.

---

## REST — WiFi / MQTT / Time

| Method | Path | Response |
|--------|------|----------|
| GET | /api/wifi/status | STA/AP JSON from WiFiManager |
| GET | /api/wifi/scan | `{ "networks": [...], "count": ≤10, "totalScanned": N }` |
| GET | /api/mqtt/status | MQTT state JSON |
| GET | /api/time | NTP status; includes `synchronized`, **`timeValid`** (same as synchronized), `iso8601`, `ntpServer` |

---

## REST — Errors / System

| Method | Path | Description |
|--------|------|-------------|
| GET | /api/errors | Error history JSON |
| POST | /api/errors/clear | Clear history |
| POST | /api/restart | Reboot |
| POST | /api/reset | Factory defaults + save + reboot (uses config lock) |

---

## REST — Firmware (Web OTA)

### POST /api/update

`multipart/form-data`, field `firmware` (binary `.bin`).

- On success: `200` `{ "success":true,"message":"Rebooting" }` then device resets.
- On failure: `500` `{ "success":false,"code":"FIRMWARE_UPDATE_FAILED","message":"..." }`; MQTT/WS restored.

---

## MQTT

Topic root: `{topicPrefix}/{deviceTag}/...` (see config). Typical subtopics:

- `.../level` — JSON water level + timestamp
- `.../status` — LWT / online JSON
- `.../command` — subscribe; payload JSON `command`: `read` | `status` | `restart`

Reconnect uses exponential backoff from 5s up to **120s** between attempts.

---

## WebSocket (port 81)

Connect to `ws://<ip>:81/`. Text JSON commands (max **512** bytes per message):

| `cmd` | Description |
|-------|-------------|
| `getStatus` | Calibration status |
| `getReading` | Single distance |
| `setOffset` | `offset` (cm) |
| `saveCalibration` | Persist offset; may return `success:false`, `code":"CONFIG_LOCKED"` |
| `resetOffset` | Zero offset |

---

## Related

- [ARCHITECTURE.md](ARCHITECTURE.md) — boot, logging, config semantics
- [CODE_STYLE.md](CODE_STYLE.md) — log levels, secrets
