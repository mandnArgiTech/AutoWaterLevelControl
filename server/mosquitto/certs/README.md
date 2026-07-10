# Mosquitto TLS certificates (generated)

Run from `server/`:

```bash
./scripts/generate-mqtt-certs.sh
```

## ESP8266 FluidLevelMonitor — what to use

| Firmware `tlsMode` | File / value | Notes |
|--------------------|--------------|-------|
| `ca` | `esp8266_ca.pem` | Upload to device `POST /api/mqtt/ca` → stored as `/mqtt_ca.pem` |
| `fingerprint` | `esp8266-fingerprint.txt` | SHA-1 of **server** cert, colon-separated hex |
| `insecure` | — | Encrypted only; no validation (dev) |

Firmware constants (do not change without matching broker):
- TLS 1.2 only (`BR_TLS12`)
- BearSSL buffers 1024 + 1024 bytes (needs broker MFLN via OpenSSL)
- Min free heap 16 KB before handshake

## Deployment defaults

Hostname: **vivasvan-tech.in** | IP: **31.97.235.233**

`./scripts/generate-mqtt-certs.sh` uses these by default. Certificate SAN includes both DNS and IP (required for `tlsMode=ca` when connecting by either).

To regenerate:

```bash
./scripts/generate-mqtt-certs.sh --force
```

Or use `tlsMode=fingerprint` with `esp8266-fingerprint.txt` when connecting by IP only.
