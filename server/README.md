# FLM Platform Server — User Guide

Water-level monitoring platform for **FluidLevelMonitor** ESP8266 devices.

One script does everything: install, uninstall, configure, and verify.

**Table of contents**

1. [What you get](#what-you-get)
2. [Requirements & install](#step-1--requirements)
3. [Run the menu (`flm-server.sh`)](#step-2--run-the-menu)
4. [Connect ESP8266](#step-5--connect-esp8266-device)
5. **[Configuration reference — every `defaults.env` setting](#configuration-reference-configdefaultsenv)** ← read this for all options
6. [Uninstall & logs](#uninstall)
7. **[New VPS — one-command install](#new-vps--one-command-install-from-your-pc)** ← fresh server
8. **[Update remote after bug fixes](#update-software-on-remote-after-bug-fixes)** ← day-to-day deploys
9. [Troubleshooting](#troubleshooting)

---

## What you get

| Part | What it does |
|------|----------------|
| **MQTT (Mosquitto)** | Receives data from ESP8266 tanks (TLS on port 8883) |
| **Database (PostgreSQL)** | Stores vendors, users, devices, readings |
| **Backend (Java)** | Login, reports, MQTT bridge |
| **Frontend (React)** | Web dashboard per vendor |

**Default server MQTT bridge:** `flmServerAdmin` / `flmPass`  
**Default device MQTT login:** `devAdmin` / `123456`  
**Default database:** `flmDB` / `flmAdmin` / `flmPass`

> Full explanation of every setting: [Configuration reference](#configuration-reference-configdefaultsenv)

---

## Step 1 — Requirements

**Default install mode is Standalone (production on VPS).**  
The installer can install most dependencies for you on **Debian/Ubuntu** (with `sudo`).

| Component | Standalone (default) | Docker-only stack |
|-----------|----------------------|-------------------|
| **Docker** | **No** | Yes — everything |
| **OpenSSL** | Yes — MQTT certificates | Yes |
| **Java 21 + Maven** | Yes — backend API on host | No |
| **Node.js 18+ + npm** | Yes — build frontend | No |
| **nginx** | Yes — serve production UI | No |

**Quick non-interactive install (auto-install missing apt packages):**

```bash
cd server
FLM_AUTO_INSTALL_DEPS=1 ./flm-server.sh install
```

Set `FLM_SKIP_DEPS=1` if you already installed everything manually.

Manual install links: [Docker](https://docs.docker.com/engine/install/) · [Node.js](https://nodejs.org/)

---

## Step 2 — Run the menu

```bash
cd server
chmod +x flm-server.sh
./flm-server.sh
```

You will see:

```
╔═══════════════════════════════════════════════════════════╗
║     FLM Platform Server Manager                           ║
╚═══════════════════════════════════════════════════════════╝

Main Menu
  1) Install
  2) Uninstall
  3) Configure
  4) Status
  5) Verify MQTT TLS
  6) Regenerate MQTT certificates
  7) View logs
  8) Install system dependencies only
  9) Remote VPS (install to /opt/flm over SSH)
  0) Exit
```

Every action writes a clear log to `server/logs/flm-server-YYYYMMDD.log`.

---

## Step 3 — Install (first time)

### Option A — Full install Standalone (default / production on VPS)

Java API, React UI, **Mosquitto**, and **PostgreSQL** all run **natively on the host** (no Docker for standalone).

1. Run `./flm-server.sh`
2. Choose **1 → Install**
3. Choose **1 → Install ALL — Standalone**

The script will automatically:

- Install missing system packages (Java 21, Maven, Node.js, nginx, Mosquitto, PostgreSQL, OpenSSL) on Debian/Ubuntu
- Generate TLS certificates (ESP8266-compatible) and `dynamic-security.json` from `.env`
- Start native Mosquitto broker (ports 1883 and 8883, Dynamic Security enabled)
- Initialize a dedicated PostgreSQL cluster under `server/postgres/data` (or `/opt/flm/postgres/data` on VPS)
- Build and start Java API (`java -jar`)
- Build React app and serve it with **nginx** on port 3000 (port **80** after remote post-install)

When finished you will see:

| Service | URL |
|---------|-----|
| Web UI | http://localhost:3000 (local) or **http://your-host/** on VPS (port 80) |
| API | http://localhost:8080/api |
| MQTT (plain) | port 1883 |
| MQTT (TLS) | port 8883 |

**Quick install without menu:**

```bash
FLM_AUTO_INSTALL_DEPS=1 ./flm-server.sh install
```

(`install` and `install-standalone` are the same — standalone is the default.)

### Option B — Remote install to VPS (from your PC)

See the full guide: **[New VPS — one-command install](#new-vps--one-command-install-from-your-pc)** (recommended for a fresh server).

Quick version:

```bash
cd server
cp config/remote.env.example config/remote.env
chmod 600 config/remote.env
# edit remote.env — host, user, password, path /opt/flm
sudo apt install openssh-client rsync sshpass
./flm-server.sh remote-install
```

Then open **http://your-vps-hostname/** (port **80**, not `:3000`).

### Option C — Full install with Docker (all services in containers)

Use this if you prefer not to install Java/Node/nginx on the host.

1. `./flm-server.sh` → **1 → 2** (Install ALL — Docker)

Or:

```bash
FLM_AUTO_INSTALL_DEPS=1 ./flm-server.sh install-docker
```

| Service | URL |
|---------|-----|
| Web UI | http://localhost:3000 (local) or **http://your-host/** on VPS (port 80) |
| API | http://localhost:8080/api |

### Option D — MQTT only (zero configuration)

If you only need the MQTT broker for ESP8266 testing:

1. `./flm-server.sh` → **1 → 3** (Install MQTT only)

Or:

```bash
./flm-server.sh mqtt-install
```

Certificates and `dynamic-security.json` are created automatically from `.env`. No OpenSSL commands needed.

**Standalone install details:**

- Installs `mosquitto` + `mosquitto-clients` (Debian/Ubuntu) if missing
- Generates `mosquitto/config/dynamic-security.json` (admin `flmDynsecAdmin`, bridge `flmServerAdmin`, device `devAdmin`)
- Renders `mosquitto/config/mosquitto-standalone.generated.conf` with absolute paths
- Starts Mosquitto as a background process (PID in `.run/mosquitto.pid`, log in `logs/mosquitto.log`)
- Disables the system `mosquitto` service if it conflicts on ports 1883/8883

**Troubleshooting Platform Admin MQTT:** If `/admin/mqtt` cannot connect, re-run `./flm-server.sh mqtt-install` or:

```bash
cd server
bash scripts/generate-dynamic-security.sh
# then restart Mosquitto via menu Configure → MQTT, or mqtt-install
```

Ensure `MQTT_DYNSEC_*` in `.env` matches `mosquitto/config/dynamic-security.json`.

**Files for ESP8266:**

| File | Use |
|------|-----|
| `mosquitto/certs/esp8266_ca.pem` | Upload to device (`tlsMode=ca`) |
| `mosquitto/certs/esp8266-fingerprint.txt` | Copy to config (`tlsMode=fingerprint`) |
| `mosquitto/certs/esp8266-mqtt-config.json` | Ready-made MQTT config snippet |

---

## New VPS — one-command install (from your PC)

Use this when you have a **fresh VPS** (Ubuntu/Debian) and want everything installed at **`/opt/flm`** without logging in manually for each step.

### What you need

| Where | What |
|-------|------|
| **Your PC** | This repo, `openssh-client`, `rsync`, `sshpass` |
| **New VPS** | Ubuntu/Debian, root SSH access, public IP or hostname |
| **Config file** | `server/config/remote.env` (gitignored — never commit) |

### Step 1 — Create `config/remote.env`

```bash
cd server
cp config/remote.env.example config/remote.env
chmod 600 config/remote.env
nano config/remote.env   # or use your editor
```

Example for a **new** VPS:

```bash
FLM_REMOTE_HOST=your-server.example.com   # or IP, e.g. 31.97.235.233
FLM_REMOTE_USER=root
FLM_REMOTE_PASSWORD=your-ssh-password     # leave empty if using SSH keys
FLM_REMOTE_PATH=/opt/flm
FLM_REMOTE_PORT=22
FLM_REMOTE_INSTALL_MODE=standalone
```

| Variable | Meaning |
|----------|---------|
| `FLM_REMOTE_HOST` | VPS hostname or IP |
| `FLM_REMOTE_USER` | SSH user (usually `root`) |
| `FLM_REMOTE_PASSWORD` | SSH password (optional if SSH key works) |
| `FLM_REMOTE_PATH` | Install directory on VPS (**default `/opt/flm`**) |
| `FLM_REMOTE_PORT` | SSH port (default `22`) |
| `FLM_REMOTE_INSTALL_MODE` | `standalone` (default) or `docker` |

Or use menu: `./flm-server.sh` → **9) Remote VPS** → **1) Configure**

> **Security:** `config/remote.env` is in `.gitignore`. Do not commit passwords. Prefer SSH keys in production.

### Step 2 — Install tools on your PC (one time)

```bash
sudo apt install openssh-client rsync sshpass
```

### Step 3 — Run one command

```bash
cd server
./flm-server.sh remote-install
```

Wait **15–20 minutes**. The script will:

| Phase | What happens |
|-------|----------------|
| **1. Sync** | `rsync` copies `server/` → `/opt/flm` on the VPS |
| **2. Install** | On VPS: installs Java, Node, nginx, Mosquitto, PostgreSQL (native), builds API + UI |
| **3. Post-install** | Automatically on VPS (no manual steps): |

**Post-install (automatic):**

- Web UI moved to **port 80** (standard HTTP — works through most firewalls)
- **ufw** firewall opened: 22, 80, 443, 8080, 1883, 8883
- **MQTT** certs + `dynamic-security.json` regenerated; native Mosquitto restarted
- **nginx** config uses absolute paths (`/opt/flm/.run/nginx.pid`)
- **Backend** restarted if MQTT was not ready on first start

### Step 4 — Open the site

| Service | URL (replace hostname) |
|---------|------------------------|
| **Web UI** | **http://your-server.example.com/** ← use port **80**, **not** `:3000` |
| API | `http://your-server.example.com:8080/api` |
| MQTT TLS (ESP8266) | `your-server.example.com:8883` |

**Default web login** (change on first login):

| Role | Username | Password | Vendor code |
|------|----------|----------|-------------|
| Super admin | `admin` | `123456` | *(empty)* |
| Vendor | `vendor` | `123456` | `demo` |

Menu shortcuts: **Main → 9) Remote VPS** or **Install → 7) Remote install**

### Update software on remote (after bug fixes)

Use **`remote-update`** when you fixed backend or frontend code — **much faster** than `remote-install` (no DB/MQTT reinstall, keeps `.env` and data).

| You changed | Command | Time (approx.) |
|-------------|---------|----------------|
| **Backend + frontend** | `./flm-server.sh remote-update` | 5–8 min |
| **Backend only** (Java API) | `./flm-server.sh remote-update-backend` | 3–5 min |
| **Frontend only** (React UI) | `./flm-server.sh remote-update-frontend` | 2–4 min |
| **Code sync only** (no rebuild) | `./flm-server.sh remote-sync` | ~30 sec |

**Database safety:** `remote-update` / `update` never run `initdb` and never delete `postgres/data`. If Postgres is down, the update only **starts the existing cluster**. First-time DB creation requires `FLM_ALLOW_DB_INIT=1` (set automatically by full `install` / `remote-install`). Intentional wipe of a non-empty data dir additionally needs `FLM_ALLOW_DB_WIPE=1`.

**What `remote-update` does:**

1. `rsync` your local `server/` → `/opt/flm` on VPS (keeps remote `.env`, MQTT certs, database)
2. SSH in and **rebuild + restart** only what you need:
   - **Backend:** `mvn package` → restart `java -jar` (starts existing Postgres if needed; **never** re-inits DB)
   - **Frontend:** `npm run build` → reload **nginx**
3. Verifies web UI still on **port 80**

Menu: **9) Remote VPS** → **5) Remote update** → choose all / backend / frontend

**On the VPS directly** (if you already SSH'd in):

```bash
cd /opt/flm
./flm-server.sh update              # backend + frontend
./flm-server.sh update-backend
./flm-server.sh update-frontend
```

**Typical workflow after fixing bugs locally:**

```bash
cd server
# fix code in backend/ or frontend/
./flm-server.sh remote-update-backend   # or remote-update-frontend / remote-update
./flm-server.sh remote-status
# open http://vivasvan-tech.in/ and test
```

> **Do not use** `remote-install` for routine updates — that reinstalls everything (~15–20 min). Use **`remote-update`**.

### Other remote commands

| Command | When to use |
|---------|-------------|
| `./flm-server.sh remote-install` | **First time** on a new VPS |
| `./flm-server.sh remote-update` | **After bug fixes** (backend + frontend) |
| `./flm-server.sh remote-update-backend` | Java API changes only |
| `./flm-server.sh remote-update-frontend` | React UI changes only |
| `./flm-server.sh remote-sync` | Push files only — no rebuild (rare) |
| `./flm-server.sh remote-status` | Check MQTT, DB, API, UI on VPS |
| `./flm-server.sh remote-uninstall` | Remove FLM from VPS |
| `./flm-server.sh remote-install-docker` | Install all-in-Docker on VPS |
| `./flm-server.sh remote-configure` | Edit `remote.env` via menu |

Menu shortcuts: **Main → 9) Remote VPS** or **Install → 7) Remote install**

### If the web page still does not open

1. Use **http://hostname/** not **http://hostname:3000/**
2. Check `./flm-server.sh remote-status`
3. On **Hostinger** (or other cloud): open ports **80**, **8080**, **1883** (plain MQTT), and **8883** (TLS MQTT) in the provider firewall panel (in addition to ufw on the server). Without **1883** / **8883** in the *cloud* panel, devices get `CONNECT_FAILED` even when Mosquitto and ufw look fine.
4. See [Troubleshooting](#troubleshooting) below

### Install directly on the VPS (SSH in yourself)

If you are already logged into the VPS (not using `remote-install` from PC):

```bash
cd /opt/flm
FLM_AUTO_INSTALL_DEPS=1 ./flm-server.sh install
```

Then set `FRONTEND_PORT=80` in `.env` and reinstall frontend, or use `remote-install` from PC which handles this automatically.

---

| Role | Username | Password | Vendor code |
|------|----------|----------|-------------|
| Super admin | `admin` | `123456` | *(leave empty)* |
| Vendor admin | `vendor` | `123456` | `demo` |

> **First login:** you will be required to change your password before accessing the dashboard.

---

## Step 5 — Connect ESP8266 device

### 5a. Set MQTT on the device

**Option A — Plain MQTT (no TLS, port 1883)** — use when ESP8266 heap is tight:

```json
"mqtt": {
  "enabled": true,
  "server": "vivasvan-tech.in",
  "port": 1883,
  "username": "devAdmin",
  "password": "123456",
  "tls": false
}
```

**Option B — TLS MQTT (port 8883)**:

```json
"mqtt": {
  "enabled": true,
  "server": "vivasvan-tech.in",
  "port": 8883,
  "username": "devAdmin",
  "password": "123456",
  "tls": true,
  "tlsMode": "ca"
}
```

Open the matching port in the **cloud firewall** (Hostinger panel): **1883** for plain, **8883** for TLS.

### 5b. Upload CA certificate to device

```bash
curl -X POST http://<device-ip>/api/mqtt/ca \
  -H 'Content-Type: text/plain' \
  --data-binary @mosquitto/certs/esp8266_ca.pem
```

Then restart the device.

### 5c. Register device in platform

After the device connects, register its `deviceTag` (format: `tank1_<chipId>`) via the API or admin UI.

### 5d. Verify TLS from server

```bash
./flm-server.sh
# Choose 5) Verify MQTT TLS
```

Or:

```bash
./scripts/verify-mqtt-tls.sh
```

---

## Configure settings

Run `./flm-server.sh` → **3) Configure**, or edit `server/.env` directly.

Settings are saved in **`server/.env`** (your live config).  
**`config/defaults.env`** is the factory template copied on first install.

For a full explanation of **every setting**, see the section below:  
**[Configuration reference (`config/defaults.env`)](#configuration-reference-configdefaultsenv)**

---

## Configuration reference (`config/defaults.env`)

This section explains **every line** in `server/config/defaults.env` — what it means, who uses it, and how to fix problems.

### How `defaults.env` and `.env` work together

```
config/defaults.env     →  copied once on first run  →  server/.env
     (in git)                    ./flm-server.sh              (your server, gitignored)
```

| File | Purpose |
|------|---------|
| `config/defaults.env` | Factory defaults shipped with the project. Change this only if you want new defaults for all fresh installs. |
| `server/.env` | **Active config** on your machine/VPS. Created automatically. Install scripts read this file. |

**To apply changes:** edit via `./flm-server.sh` → **Configure**, or edit `.env` and restart the affected service.  
**MQTT hostname/IP changes** also regenerate TLS certificates automatically.

---

### MQTT broker settings

These control **Mosquitto** (the MQTT server) and **TLS certificates** for ESP8266 devices.

#### `MQTT_SERVER_CN`

| | |
|---|---|
| **Default** | `vivasvan-tech.in` |
| **What it is** | The main hostname on the MQTT server TLS certificate (Common Name). |
| **Used by** | Certificate generator (`generate-mqtt-certs.sh`), Mosquitto TLS, ESP8266 when `tlsMode=ca` checks the server identity. |
| **Put on ESP8266** | Set `mqtt.server` in device `config.json` to this hostname **or** your VPS IP (see `MQTT_SAN_IPS`). |

**When to change:** You move to a new domain or VPS hostname.

**Troubleshooting:**

| Symptom | Fix |
|---------|-----|
| ESP8266 TLS fails with hostname error | `mqtt.server` on device must match a name in the certificate. Use `vivasvan-tech.in` or add your IP to `MQTT_SAN_IPS` and regenerate certs. |
| Certificate shows wrong name | Run `./flm-server.sh` → **6) Regenerate MQTT certificates** after updating this value. |

---

#### `MQTT_SAN_DNS`

| | |
|---|---|
| **Default** | `vivasvan-tech.in,localhost` |
| **What it is** | Extra **DNS names** allowed on the TLS certificate (Subject Alternative Names). Comma-separated. |
| **Used by** | Certificate generator only. |

**When to change:** You also connect using another DNS name (e.g. `mqtt.vivasvan-tech.in`).

**Example:**

```bash
MQTT_SAN_DNS=vivasvan-tech.in,mqtt.vivasvan-tech.in,localhost
./scripts/generate-mqtt-certs.sh --force
```

**Troubleshooting:**

| Symptom | Fix |
|---------|-----|
| TLS works on IP but not on a subdomain | Add the subdomain to `MQTT_SAN_DNS`, regenerate certs, restart Mosquitto. |

---

#### `MQTT_SAN_IPS`

| | |
|---|---|
| **Default** | `31.97.235.233,127.0.0.1` |
| **What it is** | **IP addresses** allowed on the TLS certificate. Comma-separated. |
| **Used by** | Certificate generator. Required when ESP8266 uses `tlsMode=ca` and connects by **IP address** instead of hostname. |

**When to change:** Your VPS gets a new public IP, or you test locally.

**Troubleshooting:**

| Symptom | Fix |
|---------|-----|
| Device connects to `31.97.235.233` but TLS fails in `ca` mode | Add that exact IP to `MQTT_SAN_IPS`, regenerate certs (`--force`), restart Mosquitto. |
| Easier fix for IP-only | Use `tlsMode=fingerprint` on ESP8266 with value from `mosquitto/certs/esp8266-fingerprint.txt`. |

---

#### `MQTT_USER_BRIDGE`

| | |
|---|---|
| **Default** | `flmServerAdmin` |
| **What it is** | MQTT **username for the Java backend** (MQTT bridge). |
| **Used by** | Java API connects to Mosquitto with this user to **read** device messages and **write** commands. **Not** used on ESP8266. |
| **Also in** | `application.yml`, Docker compose, Mosquitto `passwd` file, ACL (`mosquitto/acl/acl.conf`). |

**When to change:** Security hardening on production VPS.

**Troubleshooting:**

| Symptom | Fix |
|---------|-----|
| Dashboard empty but devices publish OK | Backend cannot log in to MQTT. Check `MQTT_USER_BRIDGE` / `MQTT_PASS_BRIDGE` in `.env` match Mosquitto passwd. Regenerate: `./scripts/generate-mqtt-certs.sh`. Restart backend. |
| Mosquitto health check fails | Health check uses `flmServerAdmin` — if you rename this user, update docker-compose healthchecks or regenerate passwd. |

---

#### `MQTT_PASS_BRIDGE`

| | |
|---|---|
| **Default** | `flmPass` |
| **What it is** | Password for `MQTT_USER_BRIDGE` (Java backend MQTT login). |
| **Used by** | Java API, Mosquitto authentication. |

**When to change:** Same time as `MQTT_USER_BRIDGE`.

**Troubleshooting:**

| Symptom | Fix |
|---------|-----|
| Backend log: MQTT connection refused / not authorized | Password mismatch. Run `./scripts/generate-mqtt-certs.sh` to rebuild `mosquitto/passwd/passwd`, restart Mosquitto and backend. |
| Changed password but Mosquitto still rejects | Restart Mosquitto container: `docker restart flm-mosquitto`. |

---

#### `MQTT_USER_DEVICE`

| | |
|---|---|
| **Default** | `devAdmin` |
| **What it is** | MQTT **username for ESP8266 tank devices**. |
| **Used by** | Mosquitto passwd + ACL; set on each device in `data/config.json` → `mqtt.username`. |

**When to change:** You want one shared device login for all tanks (or per-deployment policy).

**Troubleshooting:**

| Symptom | Fix |
|---------|-----|
| ESP8266 MQTT never connects (error 4/5) | Wrong username on device. Must match `MQTT_USER_DEVICE` in server `.env` / Mosquitto passwd. |
| Device connects but cannot publish | Check ACL in `mosquitto/acl/acl.conf` — user `devAdmin` must have write access to `+/water/level`. |

---

#### `MQTT_PASS_DEVICE`

| | |
|---|---|
| **Default** | `123456` |
| **What it is** | Password for `MQTT_USER_DEVICE` (ESP8266 MQTT login). |
| **Used by** | Mosquitto; device `mqtt.password` in firmware config. |

**When to change:** Before deploying devices to the field (use a stronger password in production).

**Troubleshooting:**

| Symptom | Fix |
|---------|-----|
| ESP8266 auth failed | Ensure device `mqtt.password` matches this value exactly. Regenerate Mosquitto passwd if you changed `.env`. |

---

### Database settings (PostgreSQL)

#### `POSTGRES_DB`

| | |
|---|---|
| **Default** | `flmDB` |
| **What it is** | Name of the PostgreSQL database where vendors, users, devices, and readings are stored. |
| **Used by** | PostgreSQL container, Java API JDBC URL. |

**Troubleshooting:**

| Symptom | Fix |
|---------|-----|
| Backend: database does not exist | DB name in `.env` must match what PostgreSQL was created with. Uninstall DB → Install DB again, or create DB manually. |
| Fresh install but old data appears | Old Docker volume kept. Uninstall database and answer **yes** to remove volumes. |

---

#### `POSTGRES_USER`

| | |
|---|---|
| **Default** | `flmAdmin` |
| **What it is** | PostgreSQL login username for the Java API. |
| **Used by** | PostgreSQL container, `application.yml`, Docker compose. |

**Troubleshooting:**

| Symptom | Fix |
|---------|-----|
| Backend: password authentication failed | `POSTGRES_USER` / `POSTGRES_PASSWORD` in `.env` must match PostgreSQL env vars. Recreate DB container after changes. |

---

#### `POSTGRES_PASSWORD`

| | |
|---|---|
| **Default** | `flmPass` |
| **What it is** | Password for `POSTGRES_USER`. |
| **Used by** | PostgreSQL, Java API. |

**Troubleshooting:**

| Symptom | Fix |
|---------|-----|
| API cannot connect after password change | Update both `.env` and recreate PostgreSQL container (data volume may still have old password — recreate volume if needed). |

---

#### `POSTGRES_PORT`

| | |
|---|---|
| **Default** | `5432` |
| **What it is** | TCP port PostgreSQL listens on **on your host**. |
| **Used by** | `docker-compose.postgres.yml` port mapping; Java API in standalone mode connects to `localhost:5432`. |

**Troubleshooting:**

| Symptom | Fix |
|---------|-----|
| Port already in use | Another PostgreSQL is running. Stop it or change `POSTGRES_PORT` to e.g. `5433` and update Java `application.yml` / `.env` JDBC URL. |
| Connection refused on 5432 | Run `./flm-server.sh status` — PostgreSQL container must be running. |

---

### Java API (backend) settings

#### `API_PORT`

| | |
|---|---|
| **Default** | `8080` |
| **What it is** | Port where the **REST API** listens. |
| **Used by** | Docker backend container, standalone Spring Boot, frontend nginx proxy. |

**URLs:** `http://localhost:8080/api` · Health: `http://localhost:8080/actuator/health`

**Troubleshooting:**

| Symptom | Fix |
|---------|-----|
| Port 8080 already in use | Change `API_PORT` in `.env`, reinstall backend, update frontend proxy if needed. |
| Web UI works but API calls fail | Frontend expects API on 8080 — check `API_PORT` and `docker logs flm-backend`. |

---

#### `FLM_JWT_SECRET`

| | |
|---|---|
| **Default** | `change-me-in-production-use-256-bit-secret-key-here` |
| **What it is** | Secret key used to sign **login tokens** (JWT) for web UI users. |
| **Used by** | Java security module only. |

**When to change:** **Always change on production VPS** before going live.

**Troubleshooting:**

| Symptom | Fix |
|---------|-----|
| Users logged out after restart with new secret | Expected — all old tokens invalid. Users must log in again. |
| Login works once then fails across restarts | Different `FLM_JWT_SECRET` on each backend instance — use one fixed secret in `.env`. |

---

#### `FLM_WEB_ALLOWED_ORIGINS`

| | |
|---|---|
| **Default** | `https://vivasvan-tech.in,http://vivasvan-tech.in,http://localhost:3000,http://localhost:5173` |
| **What it is** | Comma-separated list of origins allowed for **CORS** (browser → API). |
| **Used by** | Java `SecurityConfig` — required for web UI login and admin pages from your domain. |

**When to change:** Add your production URL with `https://` when TLS is enabled. Include `http://` only if you still serve plain HTTP.

---

#### `MQTT_DYNSEC_ADMIN` / `MQTT_DYNSEC_PASS`

| | |
|---|---|
| **Default** | `flmDynsecAdmin` / `flmDynsecPass` |
| **What it is** | Credentials for the **MQTT Dynamic Security** control API (platform admin MQTT page). |
| **Used by** | `MqttControlService` — must match the `flmDynsecAdmin` client in `mosquitto/config/dynamic-security.json`. |

**When to change:** Update `.env`, then run `./flm-server.sh` → **Configure → MQTT** (or `bash scripts/generate-dynamic-security.sh` and restart Mosquitto). Install regenerates `dynamic-security.json` automatically.

#### Credential architecture (who uses what)

Web login, MQTT data ingest, MQTT broker admin, and ESP8266 devices each use **different** credentials:

```
┌─────────────────┐     JWT (FLM_JWT_SECRET)      ┌──────────────────┐
│  Web browser    │ ────────────────────────────► │  Java API        │
│  (admin login)  │                               │                  │
└─────────────────┘                               │  MqttIngest      │──► flmServerAdmin ──► ingest readings
                                                  │  MqttControl     │──► flmDynsecAdmin ──► manage broker
                                                  └──────────────────┘
                                                           ▲
┌─────────────────┐                                        │
│  ESP8266        │ ───────── devAdmin ────────────────────┘
│  (sensor)       │         (publish level only)
└─────────────────┘
```

| Credential | `.env` variable | MQTT user | Purpose |
|------------|-----------------|-----------|---------|
| Web JWT | `FLM_JWT_SECRET` | — | Sign/verify browser login tokens (`admin`, `vendor`, …) |
| Data bridge | `MQTT_USER_BRIDGE` / `MQTT_PASS_BRIDGE` | `flmServerAdmin` | Backend subscribes to `+/water/#`, stores readings |
| Broker admin | `MQTT_DYNSEC_ADMIN` / `MQTT_DYNSEC_PASS` | `flmDynsecAdmin` | Platform Admin → `/admin/mqtt` (Dynamic Security API) |
| Device | `MQTT_USER_DEVICE` / `MQTT_PASS_DEVICE` | `devAdmin` | ESP8266 publishes `tank1_xxx/water/level` |

---

### React UI (frontend) settings

#### `FRONTEND_PORT`

| | |
|---|---|
| **Default** | `3000` |
| **What it is** | Port for the **web UI** — used by **standalone nginx** and **Docker frontend**. |
| **Used by** | `nginx/flm-standalone.conf` (standalone), `docker-compose.frontend.yml` (docker). |

**URL:** `http://localhost:3000`

**Troubleshooting:**

| Symptom | Fix |
|---------|-----|
| Cannot open UI on 3000 (standalone) | Check `logs/nginx-error.log` and `.run/nginx.pid`. Reinstall frontend. |
| Cannot open UI on 3000 (docker) | Check `docker ps` for `flm-frontend`. Change port in `.env` if 3000 is busy. |
| Port changed but UI still on old port | Reinstall frontend after editing `.env`. |

---

#### `FRONTEND_DEV_PORT`

| | |
|---|---|
| **Default** | `5173` |
| **What it is** | Port for **optional local development** only (`cd frontend && npm run dev`). **Not used** by the default standalone production install. |
| **Used by** | Vite dev server when you run it manually. |

**URL (manual dev only):** `http://localhost:5173`

**Troubleshooting:**

| Symptom | Fix |
|---------|-----|
| Expected production UI on 5173 | Production standalone uses `FRONTEND_PORT` (3000) via nginx — not the dev server. |
| `npm run dev` fails | Install Node.js 18+ (`FLM_AUTO_INSTALL_DEPS=1 ./flm-server.sh install`). |

---

#### `VITE_API_BASE`

| | |
|---|---|
| **Default** | `/api` |
| **What it is** | URL path the React app uses for API calls (relative to the web UI host). |
| **Used by** | Frontend build; nginx proxies `/api` → Java backend. |

**Usually leave as `/api`.** Only change if you put the API behind a different path or external URL.

**Troubleshooting:**

| Symptom | Fix |
|---------|-----|
| UI loads but all API calls 404 | nginx must proxy `/api` to backend — see `frontend/nginx.conf` and `nginx.host.conf`. |

---

### Deployment mode

#### `FLM_DEPLOY_MODE`

| | |
|---|---|
| **Default** | `standalone` |
| **Allowed values** | `standalone` \| `docker` |
| **What it is** | How **backend and frontend** run when you use menu **Install → Backend only** or **Frontend only**. |

| Mode | Meaning |
|------|---------|
| `standalone` | **Default production on VPS.** Java API runs as `java -jar`; React is built and served by **host nginx**. Mosquitto and PostgreSQL run **natively on the host** (no Docker). |
| `docker` | Backend and frontend run in **Docker containers**. |

**Note:** **Install ALL — Standalone** (menu 1) and **Install ALL — Docker** (menu 2) set this automatically.

**Troubleshooting:**

| Symptom | Fix |
|---------|-----|
| Installed backend but expected Docker container | Check `FLM_DEPLOY_MODE` in `.env`. Full install: menu **1** = Standalone, **2** = Docker. |
| Standalone backend won't start | Run `./flm-server.sh install` or install Java 21 + Maven. See `logs/backend.log`. |
| Standalone UI not on port 3000 | Standalone uses `FRONTEND_PORT` (nginx), not `FRONTEND_DEV_PORT`. Reinstall frontend after port change. |
| Missing apt packages | `FLM_AUTO_INSTALL_DEPS=1 ./flm-server.sh install` or install deps manually (see Step 1). |

---

### Quick reference table (all defaults)

| Variable | Default | Used by |
|----------|---------|---------|
| `MQTT_SERVER_CN` | `vivasvan-tech.in` | TLS cert, ESP8266 `mqtt.server` |
| `MQTT_SAN_DNS` | `vivasvan-tech.in,localhost` | TLS cert DNS names |
| `MQTT_SAN_IPS` | `31.97.235.233,127.0.0.1` | TLS cert IP addresses |
| `MQTT_USER_BRIDGE` | `flmServerAdmin` | Java backend → Mosquitto |
| `MQTT_PASS_BRIDGE` | `flmPass` | Java backend → Mosquitto |
| `MQTT_USER_DEVICE` | `devAdmin` | ESP8266 → Mosquitto |
| `MQTT_PASS_DEVICE` | `123456` | ESP8266 → Mosquitto |
| `POSTGRES_DB` | `flmDB` | PostgreSQL + Java API |
| `POSTGRES_USER` | `flmAdmin` | PostgreSQL + Java API |
| `POSTGRES_PASSWORD` | `flmPass` | PostgreSQL + Java API |
| `POSTGRES_PORT` | `5432` | PostgreSQL host port |
| `API_PORT` | `8080` | Java REST API |
| `FLM_JWT_SECRET` | *(see file, 32+ chars)* | Web login JWT signing |
| `FLM_WEB_ALLOWED_ORIGINS` | `https://vivasvan-tech.in,...` | CORS allowlist |
| `MQTT_DYNSEC_ADMIN` | `flmDynsecAdmin` | MQTT admin control API user |
| `MQTT_DYNSEC_PASS` | `flmDynsecPass` | MQTT admin control API password |
| `FRONTEND_PORT` | `3000` | Web UI (standalone nginx + Docker) |
| `FRONTEND_DEV_PORT` | `5173` | Optional `npm run dev` only |
| `VITE_API_BASE` | `/api` | Frontend API path |
| `FLM_DEPLOY_MODE` | `standalone` | Install behaviour |

---

### Configuration troubleshooting (common mistakes)

| Mistake | What happens | Fix |
|---------|--------------|-----|
| Edited `defaults.env` but not `.env` | Running server unchanged | Edit `server/.env` or use **Configure** menu; `defaults.env` only applies on **first** install. |
| Changed MQTT password, didn't regenerate passwd | Auth failures | Run `./scripts/generate-mqtt-certs.sh`, restart Mosquitto. |
| Changed DB password, didn't recreate container | API cannot connect | Uninstall → Install database. |
| Device username ≠ `MQTT_USER_DEVICE` | ESP8266 won't connect | Align device `config.json` with `.env`. |
| VPS IP changed, didn't update `MQTT_SAN_IPS` | TLS fails in `ca` mode | Update `.env`, regenerate certs, re-upload CA to devices. |
| Deleted `.env` | Fresh defaults on next run | Copy from `config/defaults.env` or run `./flm-server.sh` once. |

---

## Uninstall

Standalone / remote uninstall is a **full wipe** by default when run non-interactively:

1. Stops nginx, Java API, Mosquitto, and the native PostgreSQL cluster
2. Deletes FLM-generated data (certs, dynsec, `postgres/`, nginx conf)
3. **apt-purges** Mosquitto + PostgreSQL (removes `/etc/mosquitto` and system PG data)
4. **Deletes the install directory** (e.g. `/opt/flm`)

Java, Maven, Node, and nginx packages are left installed (often shared).

| Option | What it does |
|--------|----------------|
| Uninstall ALL (interactive) | Prompts for data purge, package purge, and install-dir delete |
| Uninstall ALL (`FLM_UNINSTALL_FULL=1` / remote) | Full wipe — no keep |
| MQTT / DB / Backend / Frontend only | Component stop + optional data purge |

**Remote full wipe (from your PC):**

```bash
./flm-server.sh remote-uninstall
```

**Local full wipe on a VPS install dir (e.g. `/opt/flm`) — destroys that folder:**

```bash
FLM_UNINSTALL_FULL=1 ./flm-server.sh uninstall
```

**Local non-interactive (purge FLM data only; keep packages + source tree):**

```bash
FLM_AUTO_YES=1 ./flm-server.sh uninstall
```

**Keep install tree but remove packages + data:**

```bash
FLM_UNINSTALL_FULL=1 FLM_UNINSTALL_REMOVE_TREE=0 ./flm-server.sh uninstall
```

After a clean remote uninstall: `/opt/flm` gone, `/etc/mosquitto` gone, Mosquitto/PostgreSQL packages gone, FLM ports free. Reinstall with `./flm-server.sh remote-install`.

Docker-mode uninstall still tears down compose services when `FLM_DEPLOY_MODE=docker`.
---

## Check status and logs

```bash
./flm-server.sh status
```

Or from menu: **4) Status** and **7) View logs**

Log files:

| File | Content |
|------|---------|
| `logs/flm-server-*.log` | Install/uninstall actions |
| `logs/backend.log` | Java API (standalone mode) |
| `logs/nginx-error.log` | Web UI nginx (standalone mode) |
| `logs/frontend.log` | Legacy / manual `npm run dev` only |
| `docker logs flm-mosquitto` | MQTT broker |

---

## Architecture (simple view)

```
ESP8266 tank  ──TLS:8883──►  Mosquitto (MQTT)
                                  │
                                  ▼
Java backend ◄──reads MQTT──  PostgreSQL
     ▲
React web UI ──login/reports──┘
```

MQTT topic from device: `tank1_abc123/water/level`

---

## Platform Administration (RBAC, MQTT, Database)

Super admins (`admin` user, `SUPER_ADMIN` role) can manage the full platform from the web UI after login.

### Admin pages

| Route | Permission | Purpose |
|-------|------------|---------|
| `/admin/roles` | `ROLE_MANAGE` | Edit role → permission matrix |
| `/admin/users` | `USER_MANAGE` | Platform-wide user management |
| `/admin/vendors` | `VENDOR_MANAGE` | Create and list vendors |
| `/admin/mqtt` | `PLATFORM_MQTT_*` | MQTT Dynamic Security — clients, device provisioning |
| `/admin/database` | `PLATFORM_DB_*` | PostgreSQL health, backups, restore |
| `/admin/sql` | `PLATFORM_SQL_*` | Guarded SQL workspace (read default; writes need step-up) |
| `/admin/audit` | `AUDIT_READ` | Immutable audit log |

### Security features

- **Short-lived JWT** (15 min) + **httpOnly refresh cookie** (rotated on each refresh)
- **TOTP MFA** required for `SUPER_ADMIN` (enroll via API after first login)
- **Step-up auth** for destructive actions (password + MFA → `X-Step-Up-Token` header)
- **Account lockout** after 5 failed logins (15 min)
- **Rate limiting** on `/api/auth/**` and `/api/admin/platform/**`
- **Flyway migrations** — schema is versioned (`db/migration/V1__*.sql`)

### MQTT Dynamic Security

Mosquitto uses the **Dynamic Security plugin**. On standalone install, `mosquitto/config/dynamic-security.json` is **generated from `.env`** (not edited by hand). The backend talks to `$CONTROL/dynamic-security/v1` — no shell scripts from the web tier.

Default dynsec admin: `flmDynsecAdmin` / `flmDynsecPass` (set via `MQTT_DYNSEC_ADMIN`, `MQTT_DYNSEC_PASS` in `.env`).

See [Credential architecture (who uses what)](#credential-architecture-who-uses-what) for how `FLM_JWT_SECRET`, `flmServerAdmin`, `flmDynsecAdmin`, and `devAdmin` relate.

### Environment variables (production)

| Variable | Purpose |
|----------|---------|
| `FLM_JWT_SECRET` | JWT signing key (min 32 chars) — **change in production** |
| `FLM_WEB_ALLOWED_ORIGINS` | CORS allowlist (e.g. `https://vivasvan-tech.in`) |
| `FLM_SERVER_ROOT` | Server install path (default `/opt/flm`) |
| `MQTT_DYNSEC_ADMIN` / `MQTT_DYNSEC_PASS` | MQTT control API credentials |

### Security runbook

1. Change default `admin` / `123456` password immediately after install.
2. Enroll MFA for super admin (`POST /api/auth/mfa/enroll` then `/mfa/confirm`).
3. Restrict `/admin/*` in nginx to trusted IPs if exposing SQL workspace publicly.
4. Take a backup before any SQL write or database restore.
5. MQTT cert rotation requires re-uploading CA to ESP8266 devices — plan maintenance windows.

### API base paths

- Auth: `/api/auth/login`, `/api/auth/refresh`, `/api/auth/step-up`, `/api/auth/mfa/*`
- Platform admin: `/api/admin/platform/**` (requires `SUPER_ADMIN` + fine-grained permissions)
- Metrics: `/actuator/prometheus` (authenticated)

---

## Troubleshooting

> **Config-related issues?** See also [Configuration troubleshooting](#configuration-troubleshooting-common-mistakes) and per-setting tables in the [Configuration reference](#configuration-reference-configdefaultsenv).

### MQTT install fails — "port already in use"

Something else is using port 1883 or 8883.

```bash
sudo ss -tlnp | grep -E '1883|8883'
```

Stop the other service, or change ports in `mosquitto/config/mosquitto.conf` and regenerate certs.

---

### ESP8266 reboots when TLS is enabled

Usually not enough free RAM during TLS handshake. The firmware guards this automatically. Ensure:

- Mosquitto 2.x with OpenSSL (Docker image `eclipse-mosquitto:2`)
- Certificates generated by `./scripts/generate-mqtt-certs.sh` (RSA 2048, not 4096)
- NTP synced on device before `tlsMode=ca`

---

### ESP8266 cannot connect — TLS / certificate error

| Problem | Fix |
|---------|-----|
| Connect by IP, `tlsMode=ca` | IP must be in cert SAN — run Configure → MQTT, or regenerate with `MQTT_SAN_IPS=31.97.235.233 ./scripts/generate-mqtt-certs.sh --force` |
| Wrong fingerprint | Use exact value from `mosquitto/certs/esp8266-fingerprint.txt` |
| CA not on device | Upload `esp8266_ca.pem` via `POST /api/mqtt/ca` |
| Clock not set | Wait for NTP sync (device waits automatically for `tlsMode=ca`) |

Test TLS from server:

```bash
./scripts/verify-mqtt-tls.sh
```

---

### Web UI opens but login fails

1. Check API is running: `./flm-server.sh status`
2. Open http://localhost:8080/actuator/health — should show `"status":"UP"`
3. Check backend logs: menu **7 → Backend log**

---

### Backend cannot connect to database

1. Ensure PostgreSQL is running: `./flm-server.sh status`
2. Check `POSTGRES_DB`, `POSTGRES_USER`, `POSTGRES_PASSWORD`, `POSTGRES_PORT` in `.env` — see [database settings](#database-settings-postgresql)
3. Restart: Uninstall database → Install database → Install backend

---

### Docker command not found

Install Docker, then log out and back in. Or run:

```bash
FLM_AUTO_INSTALL_DEPS=1 ./flm-server.sh install-deps
```

Verify:

```bash
docker --version
docker-compose --version
```

---

### Standalone install — missing Java, Node, or nginx

On Debian/Ubuntu:

```bash
FLM_AUTO_INSTALL_DEPS=1 ./flm-server.sh install-deps
```

Or from the menu: **8) Install system dependencies only**

---

### nginx config test failed — `open() ".../.run/nginx.pid" failed`

System nginx resolves **relative** paths in the config against `/usr/share/nginx`, not `/opt/flm`.  
The installer now writes **absolute** paths (`/opt/flm/.run/nginx.pid`, `/opt/flm/logs/nginx-error.log`).

**Fix:** sync the update and reinstall frontend on the VPS:

```bash
./flm-server.sh remote-sync
ssh root@vivasvan-tech.in 'cd /opt/flm && ./flm-server.sh'
# Install → 6) Frontend only
```

Or re-run: `./flm-server.sh remote-install`

---

### Certificate expired or hostname changed

```bash
./flm-server.sh
# 6) Regenerate MQTT certificates
# Then restart MQTT: 2) Uninstall MQTT → 1) Install MQTT
```

Or configure new hostname in menu **3 → MQTT** (auto-regenerates).

---

### Device sends data but dashboard is empty

1. Device `deviceTag` must be **registered** to a vendor in the platform
2. Check MQTT bridge user `flmServerAdmin` can read topics
3. View Mosquitto log: `docker logs flm-mosquitto`

---

### Full reset (start fresh)

```bash
./flm-server.sh uninstall
# Answer 'y' to remove volumes and certificates when asked
./flm-server.sh install
```

---

## File layout

```
server/
├── flm-server.sh          ← START HERE (menu)
├── config/
│   └── defaults.env       ← factory defaults (see Configuration reference)
├── .env                   ← your live settings (auto-created from defaults.env)
├── nginx/
│   ├── flm-standalone.conf.template   ← standalone production nginx template
│   └── flm-standalone.conf            ← generated at install
├── docker-compose*.yml    ← service definitions
├── scripts/
│   ├── generate-mqtt-certs.sh
│   ├── verify-mqtt-tls.sh
│   └── lib/               ← install modules
├── mosquitto/             ← MQTT config + generated certs
├── backend/               ← Java API
├── frontend/              ← React UI
└── logs/                  ← install logs
```

---

## Need help?

1. Run `./flm-server.sh status` — see what is running  
2. Check `logs/flm-server-*.log` — full install/uninstall history  
3. Run `./scripts/verify-mqtt-tls.sh` — test ESP8266 TLS compatibility  
4. Read **[Configuration reference](#configuration-reference-configdefaultsenv)** — every `defaults.env` / `.env` setting explained  
5. See **[Configuration troubleshooting](#configuration-troubleshooting-common-mistakes)** — fix wrong credentials, certs, or ports
