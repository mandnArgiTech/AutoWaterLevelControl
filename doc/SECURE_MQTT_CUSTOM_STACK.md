# FLM Secure MQTT — Custom TCP/IP Stack Guide

Everything required to connect a **custom TCP/IP + TLS stack** to the Fluid Level Monitor (FLM) **secure MQTT** broker.

> This is **MQTT over TLS**, not HTTP. HTTP is only used for the device’s local web UI / OTA.

---

## 1. Broker connection

| Parameter | Value |
|-----------|--------|
| Host (DNS) | `vivasvan-tech.in` |
| Host (IP) | `31.97.235.233` |
| Port (secure / TLS) | **8883** |
| Port (plain / no TLS) | 1883 (not for production devices) |
| Application protocol | **MQTT 3.1.1** |
| Transport | TCP → TLS → MQTT |
| TLS version | **TLS 1.2 only** |
| SNI (Server Name Indication) | Must send `vivasvan-tech.in` |
| Max MQTT packet size | **2048 bytes** |
| Max keepalive | **120 seconds** |
| Anonymous | **Disabled** — username/password required |

**Recommended production setup**

```
TCP connect → TLS 1.2 handshake (verify server) → MQTT CONNECT → PUBLISH / SUBSCRIBE
```

Prefer DNS hostname `vivasvan-tech.in` so it matches the server certificate CN/SAN.

---

## 2. Device MQTT username and password

These are the **device credentials** for ESP / custom stack clients (Dynamic Security role `device_dev`):

| Field | Value |
|-------|--------|
| **Username** | `devAdmin` |
| **Password** | `123456` |

### MQTT CONNECT fields

| Field | Value / rule |
|-------|----------------|
| Username | `devAdmin` |
| Password | `123456` |
| Client ID | Unique per device, e.g. `FluidMonitor_<chipId>` |
| Clean session | `true` |
| Keepalive | 30–60 s recommended (max 120) |

> Do **not** use platform bridge credentials (`flmServerAdmin`) on field devices.

---

## 3. Trusting the broker (TLS server authentication)

Pick **one** trust mode.

### Mode A — CA certificate (recommended)

| Item | Value |
|------|--------|
| `tls` | `true` |
| `tlsMode` | `ca` |
| CA file | `server/mosquitto/certs/esp8266_ca.pem` (on VPS: `/opt/flm/mosquitto/certs/esp8266_ca.pem`) |
| Requirement | Device clock must be correct (NTP) before TLS verify |

Upload example (if using FLM device UI API):

```bash
curl -X POST http://DEVICE_IP/api/mqtt/ca \
  -H 'Content-Type: text/plain' \
  --data-binary @server/mosquitto/certs/esp8266_ca.pem
```

### Mode B — Server certificate fingerprint

| Item | Value |
|------|--------|
| `tls` | `true` |
| `tlsMode` | `fingerprint` |
| Fingerprint (SHA-1, colon form) | See `server/mosquitto/certs/esp8266-fingerprint.txt` |

Example (local generated certs — **regenerate may change this**):

```text
1B:85:21:25:65:C2:3E:1C:9C:C5:18:04:C7:AF:8C:3D:7B:31:A8:A8
```

Refresh from server cert:

```bash
openssl x509 -in server/mosquitto/certs/server.crt -noout -fingerprint -sha1
```

### Mode C — Insecure (dev only)

| Item | Value |
|------|--------|
| `tlsMode` | `insecure` |
| Meaning | Encryption only — **no** cert verification |

**Client certificates are not required.** Authentication is MQTT username/password after TLS.

---

## 4. Ready-to-paste device MQTT config

Secure TLS config matching `server/mosquitto/certs/esp8266-mqtt-config.json`:

```json
{
  "enabled": true,
  "server": "vivasvan-tech.in",
  "port": 8883,
  "username": "devAdmin",
  "password": "123456",
  "clientId": "FluidMonitor",
  "deviceName": "tank2",
  "topicPrefix": "water",
  "publishInterval": 15000,
  "tls": true,
  "tlsMode": "ca",
  "fingerprint": "1B:85:21:25:65:C2:3E:1C:9C:C5:18:04:C7:AF:8C:3D:7B:31:A8:A8"
}
```

For fingerprint mode, set `"tlsMode": "fingerprint"` and keep the fingerprint value above (or the live VPS value after cert check).

---

## 5. Topics

`deviceTag` = `{deviceName}_{chipIdHex}`  
Example: `deviceName=tank2`, chipId=`34ea20` → **`tank2_34ea20`**

| Direction | Topic | QoS | Purpose |
|-----------|--------|-----|---------|
| Device → Broker | `{deviceTag}/water/level` | 0 or 1 | Level telemetry |
| Device → Broker | `{deviceTag}/water/status` | 0 or 1 | Online / health (often retained) |
| Broker → Device | `{deviceTag}/water/command` | 1 | Commands from platform |

ACL (DynSec role `device_dev`) allows publish on `+/water/level` and `+/water/status`, and subscribe on `+/water/command`.

### Platform registration (required)

The cloud bridge **ignores** unregistered tags. After first connect, register e.g. `tank2_34ea20` via UI or:

```http
POST /api/devices
Authorization: Bearer <vendor JWT>
Content-Type: application/json

{
  "vendorId": "<demo-vendor-uuid>",
  "deviceTag": "tank2_34ea20",
  "displayName": "Tank 2",
  "chipId": "34ea20"
}
```

Default vendor login for platform UI: username `vendor`, password `123456`, vendor code `demo`.

---

## 6. Payload formats

### Publish — `{deviceTag}/water/level`

Keep under ~1024–1500 bytes (buffer/broker limits). Typed fields the backend stores:

```json
{
  "device": "Water Tank Monitor",
  "deviceTag": "tank2_34ea20",
  "deviceName": "tank2",
  "chipId": "34ea20",
  "timestamp": "2026-07-15T09:00:00+05:30",
  "uptimeMs": 3600000,
  "uptime": "1h 0m 0s",
  "level": {
    "percentFilled": 42.5,
    "percentRemaining": 57.5,
    "waterHeightCm": 63.9,
    "waterHeightMm": 639,
    "volumeLiters": 850.0,
    "volumeRemaining": 1150.0,
    "state": "ok"
  },
  "sensor": {
    "type": "A02YYUW Ultrasonic",
    "ok": true,
    "distanceCm": 86.4,
    "distanceMm": 864,
    "temperatureC": 29.0,
    "humidityPct": 40.0,
    "valid": true
  },
  "tank": {
    "type": "circular",
    "heightCm": 150.3,
    "heightMm": 1503,
    "totalVolumeLiters": 2000.0
  },
  "battery": {
    "voltage": 3.90,
    "percent": 80
  }
}
```

### Publish — `{deviceTag}/water/status`

```json
{
  "online": true,
  "firmware": "FLM-1.1.0",
  "deviceTag": "tank2_34ea20",
  "deviceName": "tank2",
  "chipId": "34ea20",
  "ip": "192.168.68.68",
  "rssi": -45,
  "uptime": 3600000,
  "freeHeap": 20000
}
```

### Subscribe — `{deviceTag}/water/command`

```json
{ "command": "read" }
```

Supported commands: `read`, `status`, `restart`.

---

## 7. Checklist for your custom stack

1. Resolve / connect TCP to `vivasvan-tech.in:8883`
2. TLS 1.2 handshake with SNI = `vivasvan-tech.in`
3. Verify with CA PEM **or** SHA-1 fingerprint
4. MQTT CONNECT with username `devAdmin`, password `123456`, unique clientId
5. SUBSCRIBE `{deviceTag}/water/command`
6. PUBLISH `{deviceTag}/water/level` and `{deviceTag}/water/status`
7. Register `deviceTag` on the FLM platform
8. Keep packets ≤ 2048 bytes; sync NTP before CA verify

---

## 8. Other MQTT users (reference — not for tank devices)

| Username | Password | Role |
|----------|----------|------|
| `devAdmin` | `123456` | Device publish/subscribe (use this on tanks) |
| `flmServerAdmin` | `flmPass` | Java bridge (server only) |
| `flmDynsecAdmin` | `flmDynsecPass` | Dynamic Security admin (server only) |
| `vendor_demo` | (see server `.env`) | Vendor MQTT role |

---

## 9. Files on disk

| File | Purpose |
|------|---------|
| `server/mosquitto/certs/esp8266_ca.pem` | CA to trust the broker |
| `server/mosquitto/certs/esp8266-fingerprint.txt` | SHA-1 fingerprint mode |
| `server/mosquitto/certs/esp8266-mqtt-config.json` | Sample device MQTT JSON |
| `server/mosquitto/certs/server.crt` / `server.key` | Broker server identity (host only) |

---

## 10. Quick TLS smoke test (Linux)

```bash
openssl s_client -connect vivasvan-tech.in:8883 \
  -servername vivasvan-tech.in \
  -tls1_2 \
  -CAfile server/mosquitto/certs/esp8266_ca.pem
```

```bash
mosquitto_pub -h vivasvan-tech.in -p 8883 \
  --cafile server/mosquitto/certs/esp8266_ca.pem \
  -u devAdmin -P '123456' \
  -t 'tank2_34ea20/water/status' \
  -m '{"online":true,"deviceTag":"tank2_34ea20"}'
```

---

*Generated for custom TCP/IP + TLS integration with FLM Mosquitto on `vivasvan-tech.in:8883`.*
