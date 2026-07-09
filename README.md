# FluidLevelMonitor

**ESP8266 water-level monitor** for Sintex-style tanks and similar setups: distance → fill %, height, volume; WiFi + MQTT + web UI + REST; optional multi-sensor support.

[![Platform](https://img.shields.io/badge/platform-ESP8266-orange)](https://www.espressif.com/)
[![Framework](https://img.shields.io/badge/framework-Arduino-00979D)](https://www.arduino.cc/)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

---

## Table of contents

1. [What you get](#what-you-get)
2. [Hardware](#hardware)
3. [Quick start](#quick-start)
4. [Build & flash](#build--flash)
5. [Firmware updates (OTA)](#firmware-updates-ota)
6. [Configuration & access](#configuration--access)
7. [REST API (summary)](#rest-api-summary)
8. [MQTT](#mqtt)
9. [WebSocket calibration](#websocket-calibration)
10. [Logging & diagnostics](#logging--diagnostics)
11. [Project layout](#project-layout)
12. [Documentation suite](#documentation-suite)
13. [Security notes](#security-notes)
14. [Troubleshooting](#troubleshooting)
15. [License & contributing](#license--contributing)

---

## What you get

| Area | Details |
|------|---------|
| **Measurement** | Percent filled / remaining, water height (cm), volume (L); tank states (empty, low, normal, high, full, etc.) |
| **Filtering** | Median → moving average → optional Kalman (configurable) for stable surface readings |
| **Sensors** | Pluggable drivers: **US-100**, **HC-SR04**, **TF-Luna**, **XKC-KD200** (see [doc/SENSOR_GUIDE.md](doc/SENSOR_GUIDE.md)) |
| **Connectivity** | WiFi STA with **AP fallback** (captive-style DNS), **mDNS**, **NTP** |
| **Integration** | **MQTT** (JSON + LWT), **REST API**, **Web UI** (LittleFS, gzip’d assets at build) |
| **Calibration** | **WebSocket** on port **81** for live distance + offset save |
| **Reliability** | Structured logging, error history, atomic config save, config write locking, Web + Arduino OTA |
| **Tooling** | PlatformIO, auto build number, optional `pio check` (cppcheck) |

Runtime model: **single cooperative `loop()`** on ESP8266 (no RTOS threads); WiFi/MQTT/HTTP interleave via callbacks and `yield()`.

---

## Hardware

### Minimum bill of materials

| Item | Notes |
|------|--------|
| **NodeMCU v2** (ESP8266, e.g. ESP-12E) | 80 MHz, ~80 KB RAM |
| **Distance sensor** | US-100 recommended (UART); others per sensor guide |
| **Power** | Stable 5 V USB or supply; adequate current for WiFi peaks |
| **Wiring** | Per sensor; default US-100: **D1/GPIO5 ↔ sensor RX**, **D2/GPIO4 ↔ sensor TX**, GND, VCC (see sensor datasheet for UART mode jumper) |

### Default tank preset (circular / Sintex-style)

| Parameter | Default |
|-----------|---------|
| Diameter | 1350 mm |
| Height | 1704.5 mm |
| Volume (computed) | ~2438 L |

Override via web UI or REST (`/api/config/tank`).

---

## Quick start

1. **Flash firmware** and **LittleFS** (see below).
2. Power on; if no STA credentials (or connect fails), device opens **AP** (default SSID/password in `data/config.json` — often `FluidMonitor-AP` / `12345678`).
3. Browse **`http://192.168.4.1`** → configure WiFi, tank, sensor, MQTT.
4. On LAN: **`http://<device-ip>/`** or **`http://<hostname>.local/`** (if mDNS works on your network).

Full walkthrough: **[doc/USER_GUIDE.md](doc/USER_GUIDE.md)**.

---

## Build & flash

**Prerequisites:** [PlatformIO](https://platformio.org/) (CLI or VS Code), Python 3 (pre-build scripts).

```bash
# Compile
pio run

# Serial upload (USB)
pio run -t upload

# LittleFS (web UI, default config, errors.json) — required for first use
pio run -t uploadfs

# Serial monitor @ 115200
pio device monitor
```

**Optional build flags** (in `platformio.ini` → `build_flags`):

- `-D FLM_LOG_VERBOSE` — DEBUG/TRACE log lines on Serial  
- `-D FLM_DEBUG` — lightweight assertions (`FLM_ASSERT`)

**Static analysis:**

```bash
pio check -e nodemcuv2
```

(Build also runs `build_increment.py` and `gzip_www.py`: compressed `index.html.gz` where applicable.)

---

## Firmware updates (OTA)

| Method | How |
|--------|-----|
| **Web UI** | **Firmware** tab → upload `firmware.bin` → **POST `/api/update`**. MQTT + WebSocket are paused during upload to free heap; on failure services are restored. |
| **ArduinoOTA** | IDE / `pio run -t upload --upload-port <IP>`. Before transfer: MQTT disconnect, WebSocket stop, **HTTP server stop** to maximize heap for OTA. |

See **[doc/ARCHITECTURE.md](doc/ARCHITECTURE.md)** for OTA flow and memory considerations.

---

## Configuration & access

- **Storage:** JSON on **LittleFS** (`/config.json`), with backup `config.bak`; saves use a temp file + rename (atomic).
- **Concurrent writes:** One config transaction at a time; overlapping REST/WebSocket saves may get **503** / `CONFIG_LOCKED`.
- **POST body limit:** **12288 bytes** for full/section config JSON.

| Access | URL |
|--------|-----|
| AP mode | `http://192.168.4.1` |
| STA | `http://<ip>/` or `http://<hostname>.local/` |

---

## REST API (summary)

Base: `http://<host>/api/…`

| Group | Examples |
|-------|----------|
| **Status** | `GET /api/status` (heap, fragmentation, max free block, level, connection), `/api/level`, `/api/sensor`, `/api/info` |
| **Config** | `GET|POST /api/config`, `GET|POST /api/config/{wifi\|mqtt\|tank\|sensor\|system}` |
| **Network** | `/api/wifi/status`, `/api/wifi/scan` (max **10** SSIDs in response), `/api/mqtt/status`, `/api/time` (`timeValid` / NTP) |
| **System** | `/api/errors`, `POST /api/errors/clear`, `POST /api/restart`, `POST /api/reset` |
| **OTA** | `POST /api/update` (multipart firmware) |

Stable error JSON (many failures): `{ "success": false, "code": "…", "message": "…" }`.

**Authoritative schema and limits:** **[doc/API_REFERENCE.md](doc/API_REFERENCE.md)**.

---

## MQTT

Topics are **device-scoped**, typically:

`{topicPrefix}/{deviceName}_{chipId}/…`

Subtopics include **`level`**, **`status`** (LWT), **`command`** (subscribe). Payloads are JSON; timestamps depend on NTP when synchronized.

Commands (example topic suffix `…/command`):

```json
{"command": "read"}
{"command": "status"}
{"command": "restart"}
```

Reconnect uses **exponential backoff** (capped). Details: [doc/API_REFERENCE.md](doc/API_REFERENCE.md), [doc/USER_GUIDE.md](doc/USER_GUIDE.md).

### MQTT over TLS

Set in the `mqtt` config section (web UI, `POST /api/config/mqtt`, or `config.json`):

```json
{"tls": true, "port": 8883, "tlsMode": "insecure"}
```

| `tlsMode` | Validation | Notes |
|---|---|---|
| `insecure` | none (traffic still encrypted) | Default; no setup needed |
| `fingerprint` | SHA-1 cert pinning | Set `fingerprint` (e.g. `"AA:BB:…"`); must be updated on cert renewal |
| `ca` | full X.509 chain | Upload CA PEM via `POST /api/mqtt/ca` (body = PEM); waits for NTP sync before connecting |

TLS is tuned for the ESP8266's small heap: **MFLN 1 KB buffers** (broker must support MFLN — any OpenSSL 1.1.1+ based broker such as Mosquitto on a modern VPS does), **TLS 1.2 only**, **session resumption** (reconnects skip the slow handshake), and a **free-heap guard** that defers the handshake instead of crashing when memory is low. First handshake takes ~1–3 s (CPU runs at 160 MHz to halve this); resumed handshakes are near-instant.

---

## WebSocket calibration

- **URL:** `ws://<device-ip>:81/`
- **Purpose:** Live distance, set offset (cm), save offset to config, reset offset.
- **Limits:** JSON commands capped (**512 bytes**); save respects config lock (`CONFIG_LOCKED` if busy).

---

## Logging & diagnostics

- Serial **115200**, lines like **`[FLM][I][WiFi] message`** (I=INFO, W=WARN, E=ERROR; D/T need `FLM_LOG_VERBOSE`).
- **Do not log or share** full config dumps publicly; firmware avoids printing WiFi/MQTT passwords by design.
- **`GET /api/status`**: heap, fragmentation, max free block for field diagnosis.

---

## Project layout

```
FluidLevelMonitor/
├── data/                      # LittleFS image source
│   ├── config.json
│   ├── errors.json
│   └── index.html (+ .gz from build)
├── doc/                       # Full doc suite (see below)
├── scripts/
│   ├── build_increment.py
│   ├── gzip_www.py
│   └── build_number.json
├── src/
│   ├── main.cpp
│   ├── version.h
│   ├── config/                # ConfigManager (LittleFS, atomic save, clamps)
│   ├── network/
│   │   ├── WiFiManager        # STA/AP, OTA, DNS, mDNS
│   │   ├── MQTTManager
│   │   ├── WebServer.cpp      # Core HTTP + static files
│   │   ├── WebServerApi.cpp   # REST handlers
│   │   ├── WebServerFirmware.cpp  # POST /api/update
│   │   └── CalibrationWS.cpp  # Port 81
│   ├── sensor/                # ISensor, factory, filters, drivers
│   ├── tank/                  # TankCalculator
│   └── utils/                 # ErrorHandler, TimeManager, Log, FlmTime
├── platformio.ini
└── README.md
```

---

## Documentation suite

| Document | Purpose |
|----------|---------|
| [doc/README.md](doc/README.md) | Doc index |
| [doc/USER_GUIDE.md](doc/USER_GUIDE.md) | End-user setup & daily use |
| [doc/DEVELOPER_GUIDE.md](doc/DEVELOPER_GUIDE.md) | Code layout, build, extending sensors |
| [doc/API_REFERENCE.md](doc/API_REFERENCE.md) | REST, MQTT, WebSocket, limits |
| [doc/ARCHITECTURE.md](doc/ARCHITECTURE.md) | Boot, modules, OTA, threat model |
| [doc/CODE_STYLE.md](doc/CODE_STYLE.md) | Comments, logging policy |
| [doc/SENSOR_GUIDE.md](doc/SENSOR_GUIDE.md) | Sensor choice & wiring |
| [doc/TROUBLESHOOTING.md](doc/TROUBLESHOOTING.md) | Common failures |
| [doc/MANUAL_TEST_MATRIX.md](doc/MANUAL_TEST_MATRIX.md) | Manual QA checklist |

---

## Security notes

- **No HTTP authentication** — treat the device as **trusted LAN only** (or isolate VLAN / firewall).
- Change default **AP password** before deployment.
- **MQTT TLS** is built in (BearSSL); prefer `tlsMode: "ca"` or `"fingerprint"` over `"insecure"` when the broker is on the public internet.
- Rotate any **Git remote credentials** if they were ever embedded in URLs.

---

## Troubleshooting

| Symptom | Pointer |
|---------|---------|
| Sensor dead / timeouts | Wiring, UART mode, [doc/TROUBLESHOOTING.md](doc/TROUBLESHOOTING.md) |
| WiFi / AP issues | Credentials, RSSI, AP fallback |
| MQTT silent | Broker reachability, `publishInterval`, backoff |
| OTA fails / crash | Free heap; use Web OTA or ensure ArduinoOTA path (services stopped) |
| Config won’t save | **507** FS full; **503** locked — retry |
| Corrupt config | Backup restore on boot; see ARCHITECTURE |

---

## Error codes

Central list in **`data/errors.json`** (loaded on demand + cached lookups). Ranges: 0xx system, 1xx sensor, 2xx tank, 3xx WiFi, 4xx MQTT, 5xx config/FS, 6xx web, 7xx time.

---

## License & contributing

**MIT** — see [LICENSE](LICENSE) if present in the repo.

Contributions via issues and pull requests are welcome; run **`pio run`** and consider **`pio check`** before submitting.

---

**FluidLevelMonitor** — tank level on ESP8266 with MQTT, REST, and a maintainable modular codebase.
