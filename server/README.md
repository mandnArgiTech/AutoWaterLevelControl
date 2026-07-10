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
7. [Troubleshooting](#troubleshooting)

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
| **Docker** | Yes — MQTT + PostgreSQL | Yes — everything |
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
  0) Exit
```

Every action writes a clear log to `server/logs/flm-server-YYYYMMDD.log`.

---

## Step 3 — Install (first time)

### Option A — Full install Standalone (default / production on VPS)

Java API and React UI run **on the host** (production JAR + nginx).  
MQTT and PostgreSQL run in **Docker** (reliable, easy TLS).

1. Run `./flm-server.sh`
2. Choose **1 → Install**
3. Choose **1 → Install ALL — Standalone**

The script will automatically:

- Install missing system packages (Java 21, Maven, Node.js, nginx, Docker, OpenSSL) on Debian/Ubuntu
- Generate TLS certificates (ESP8266-compatible)
- Start MQTT broker (ports 1883 and 8883)
- Start PostgreSQL
- Build and start Java API (`java -jar`)
- Build React app and serve it with **nginx** on port 3000

When finished you will see:

| Service | URL |
|---------|-----|
| Web UI | http://localhost:3000 |
| API | http://localhost:8080/api |
| MQTT (plain) | port 1883 |
| MQTT (TLS) | port 8883 |

**Quick install without menu:**

```bash
FLM_AUTO_INSTALL_DEPS=1 ./flm-server.sh install
```

(`install` and `install-standalone` are the same — standalone is the default.)

### Option B — Full install with Docker (all services in containers)

Use this if you prefer not to install Java/Node/nginx on the host.

1. `./flm-server.sh` → **1 → 2** (Install ALL — Docker)

Or:

```bash
FLM_AUTO_INSTALL_DEPS=1 ./flm-server.sh install-docker
```

| Service | URL |
|---------|-----|
| Web UI | http://localhost:3000 |
| API | http://localhost:8080/api |

### Option C — MQTT only (zero configuration)

If you only need the MQTT broker for ESP8266 testing:

1. `./flm-server.sh` → **1 → 3** (Install MQTT only)

Or:

```bash
./flm-server.sh mqtt-install
```

Certificates are created automatically. No OpenSSL commands needed.

**Files for ESP8266:**

| File | Use |
|------|-----|
| `mosquitto/certs/esp8266_ca.pem` | Upload to device (`tlsMode=ca`) |
| `mosquitto/certs/esp8266-fingerprint.txt` | Copy to config (`tlsMode=fingerprint`) |
| `mosquitto/certs/esp8266-mqtt-config.json` | Ready-made MQTT config snippet |

---

## Step 4 — Log in to the web UI

| Role | Email | Password | Vendor code |
|------|-------|----------|-------------|
| Super admin | `admin` | `123456` | *(leave empty)* |
| Vendor admin | `vendor` | `123456` | `demo` |

> **First login:** you will be required to change your password before accessing the dashboard.

---

## Step 5 — Connect ESP8266 device

### 5a. Set MQTT on the device

In device `config.json` (or web UI):

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
| `standalone` | **Default production on VPS.** Java API runs as `java -jar`; React is built and served by **host nginx**. MQTT + PostgreSQL use Docker. |
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
| `FLM_JWT_SECRET` | *(see file)* | Web login tokens |
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

Run `./flm-server.sh` → **2) Uninstall**

| Option | What it removes |
|--------|-----------------|
| Uninstall ALL | Everything |
| MQTT only | Mosquitto container (asks about certs/volumes) |
| Database only | PostgreSQL (asks about data) |
| Backend / Frontend | Individual services |

**Quick uninstall:**

```bash
./flm-server.sh uninstall
```

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
