# Deploy & OTA runbook — remote host + firmware

Day-to-day guide to **redeploy the server platform** to a VPS and **burn / OTA firmware** to ESP8266 devices.  
Full menus and edge cases: [server/README.md](../server/README.md), [TROUBLESHOOTING.md](TROUBLESHOOTING.md), root [README.md](../README.md).

---

## A. Remote server (VPS) — platform

### A.0 Prerequisites (your PC)

```bash
sudo apt install openssh-client rsync sshpass
cd server
cp config/remote.env.example config/remote.env   # once
chmod 600 config/remote.env
# Edit: FLM_REMOTE_HOST, FLM_REMOTE_USER, FLM_REMOTE_PASSWORD (or use SSH keys),
#       FLM_REMOTE_PATH=/opt/flm, FLM_REMOTE_INSTALL_MODE=standalone
```

Never commit `server/config/remote.env` (gitignored).

### A.1 First time on a new VPS

```bash
cd server
./flm-server.sh remote-install
```

Takes ~15–20 min. Syncs `server/` → `/opt/flm`, installs Java/Node/nginx/Mosquitto/Postgres, builds API + UI, opens firewall ports, puts UI on **port 80**.

| URL | Example |
|-----|---------|
| Web UI | `http://YOUR_HOST/` |
| API | `http://YOUR_HOST:8080/api` |
| MQTT TLS | `YOUR_HOST:8883` |

Default logins (change on first login): `admin` / `123456` (no vendor code); `vendor` / `123456` + vendor code `demo`.

### A.2 After code changes (routine redeploy) — use this for M1+

**Do not** re-run `remote-install` for normal updates (slow; reinstalls stack).

```bash
cd server
git pull   # or work from your feature branch locally

# Backend (Java) + frontend (React) — usual choice after M1 commits
./flm-server.sh remote-update

# Or only what changed:
./flm-server.sh remote-update-backend    # flm-api / Flyway / MQTT bridge
./flm-server.sh remote-update-frontend   # React UI only
./flm-server.sh remote-sync              # rsync files only, no rebuild
```

| Command | Keeps DB / `.env` / certs? | Typical time |
|---------|----------------------------|--------------|
| `remote-update` | Yes | 5–8 min |
| `remote-update-backend` | Yes | 3–5 min |
| `remote-update-frontend` | Yes | 2–4 min |
| `remote-install` | Fresh install path | 15–20 min — **new VPS only** |

**Flyway:** backend restart applies new migrations (V7–V10, etc.) automatically.  
**Database:** `remote-update` never `initdb`s and never deletes `postgres/data`.

### A.3 Smoke after remote update

1. Open `http://YOUR_HOST/` — login works.  
2. `curl -sf http://YOUR_HOST:8080/actuator/health` → `{"status":"UP"}`.  
3. Dashboard still shows devices / levels.  
4. Optional: `/setup` wizard and `GET /api/capabilities`.

### A.4 Local server (same machine as the repo)

```bash
cd server
./flm-server.sh          # menu
# or first install: FLM_AUTO_INSTALL_DEPS=1 ./flm-server.sh install
```

---

## B. Firmware — USB burn and remote OTA

### B.1 Build

```bash
# From repo root
pio run -e sensor_only_d1mini
# Binary: .pio/build/sensor_only_d1mini/firmware.bin
```

Other envs: see `platformio.ini` (motor roles, OTA env `sensor_only_d1mini_ota`).

### B.2 USB flash (always works; use if OTA fails)

```bash
pio run -e sensor_only_d1mini -t upload
pio run -e sensor_only_d1mini -t uploadfs   # LittleFS (web UI + config)
```

### B.3 OTA over Wi‑Fi (ArduinoOTA / PlatformIO)

1. Device must be on the same LAN, ArduinoOTA enabled, stable (not reboot-looping).  
2. Set **your PC’s LAN IP** in `platformio.ini` under `sensor_only_d1mini_ota`:

```ini
upload_flags =
	--host_ip=192.168.x.y
```

3. Allow reverse TCP (ESP → PC) if UFW is active:

```bash
sudo ufw allow from 192.168.x.0/24 comment 'FLM ArduinoOTA'
sudo ufw reload
```

4. Prefer the preflight helper:

```bash
scripts/ota_preflight.sh <device-ip>           # check only
scripts/ota_preflight.sh <device-ip> upload    # firmware OTA
scripts/ota_preflight.sh <device-ip> uploadfs  # filesystem OTA
```

Or directly:

```bash
pio run -e sensor_only_d1mini_ota -t upload   --upload-port <device-ip>
pio run -e sensor_only_d1mini_ota -t uploadfs --upload-port <device-ip>
```

### B.4 OTA via device Web UI

1. Open `http://<device-ip>/` → **Firmware** tab.  
2. Upload `firmware.bin` (from `.pio/build/.../firmware.bin`).  
3. Device pauses MQTT/WebSocket during update to free heap.

### B.5 After firmware flash — MQTT to the VPS

1. Point device MQTT `server` at the VPS hostname/IP (TLS port **8883** if used).  
2. For `tlsMode=ca`, upload CA from `server/mosquitto/certs/esp8266_ca.pem` (or device `POST /api/mqtt/ca`).  
3. On connect, device publishes retained `{deviceTag}/system/announce` (M1).  
4. Register from pending list or `/setup` wizard on the platform UI.

### B.6 If OTA says “No response from device”

See [TROUBLESHOOTING.md — OTA "No response from device"](TROUBLESHOOTING.md#ota-no-response-from-device): almost always **PC firewall** blocking ESP→PC TCP, or wrong `--host_ip`.

### B.7 If device reboot-loops after OTA (OOM)

See [TROUBLESHOOTING.md — OOM reboot loop after OTA](TROUBLESHOOTING.md#oom-reboot-loop-after-ota). Recover with **USB** flash of a known-good build; use `ota_preflight.sh` once stable.

---

## C. Recommended order after pulling M1 (or later) commits

```text
1. git pull on your PC
2. cd server && ./flm-server.sh remote-update
3. Smoke VPS UI + /actuator/health
4. Build firmware: pio run -e sensor_only_d1mini
5. OTA or USB flash devices that need announce/health fields
6. Confirm pending/announce and dashboard levels still update
```

---

## D. Quick command cheat sheet

| Goal | Command |
|------|---------|
| First VPS install | `cd server && ./flm-server.sh remote-install` |
| Redeploy API+UI | `cd server && ./flm-server.sh remote-update` |
| Redeploy API only | `./flm-server.sh remote-update-backend` |
| Redeploy UI only | `./flm-server.sh remote-update-frontend` |
| USB firmware | `pio run -e sensor_only_d1mini -t upload` |
| USB LittleFS | `pio run -e sensor_only_d1mini -t uploadfs` |
| OTA check + flash | `scripts/ota_preflight.sh <ip> upload` |
| OTA filesystem | `scripts/ota_preflight.sh <ip> uploadfs` |
