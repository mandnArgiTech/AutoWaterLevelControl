# FluidLevelMonitor — Project Status (Clear / Step-by-Step)

Last updated: 2026-09-09  
Source of truth for implementation: git commit `9295dc2` + Flyway migrations V1–V10  
Note: checklist marks inside `BACKEND-REDESIGN-STORIES.md` may still be stale; prefer this file + code/DB.

---

## 1. Aim and goal

### Product aim

Turn FluidLevelMonitor from a “water tank MQTT + dashboard” system into a **generic multi-vendor IoT platform** for house / building / utility / irrigation water use cases — while keeping today’s water monitoring working.

### Concrete redesign goals (locked)

1. Devices report **capabilities** (water level, env, soil, motor, health…), not only “level”.
2. The real world is modeled as **sites → assets → flows → bindings** (tanks/pumps linked to device channels).
3. Vendors have a **type** (House / Building / Network / Field) that changes landing UX.
4. **Hard safety** stays in firmware; **soft control policies** come later in the cloud (M2).
5. Work lives on **`server/` only** (Track B). SudarshanChakra Android/backend is deferred.

### Big-start choice

Ship Foundations + Capabilities + Topology together as **Milestone M1**, then Control (**M2**), then Transport/Scale (**M3**).

Reference docs:

- [BACKEND-REDESIGN.md](BACKEND-REDESIGN.md)
- [BACKEND-REDESIGN-STORIES.md](BACKEND-REDESIGN-STORIES.md)

---

## 2. Roadmap in order (what “complete” means)

```text
DONE     Decisions (D)     — which stack, which client, architecture locks
DONE     Phase 0 / Safety  — heap/OTA/pending/backup so we don’t brick devices/DB
DONE     Milestone M1      — Foundations + Capabilities + Topology
NOT YET  Milestone M2      — Control policies, coordination, interlocks
NOT YET  Milestone M3      — LoRa/4G, edge autonomy, simulation, predictive
ONGOING  Tests (X.3)       — should grow with every milestone
```

```mermaid
flowchart LR
  D[Decisions] --> S[Phase0_Safety]
  S --> M1[Milestone_M1]
  M1 --> M2[Milestone_M2_Control]
  M2 --> M3[Milestone_M3_Scale]
```

---

## 3. Where we are right now

| Check | Result |
|--------|--------|
| Latest git commit | `9295dc2` — *feat: Milestone M1 foundations, capabilities, and topology* (2026-08-20) |
| Branch | `feature/fluid-level-monitor-esp8266` — pushed to origin |
| Code present | V7–V10 migrations, capability handlers, topology APIs, `/setup` wizard, HeapMonitor, announce |
| Postgres Flyway | History includes migrations through topology (V7…V10 on disk and applied historically) |
| API process | May be stopped locally — start with server scripts before smoke testing |
| Story checklist file | May still show F/C/L as ☐; **this status file + code/DB are authoritative** |

**Plain English:**  
**M1 is implemented and pushed.** Code and database schema are ahead of any stale checkbox marks in the stories doc. If the API is down, that is a runtime/start issue, not missing M1 work.

---

## 4. Completion status — step by step

### Step A — Decisions → 100% done

- Use standalone `server/` platform.
- Frontend = `server/frontend`.
- Hybrid telemetry, topology yes, LoRa parked, control policies later.

### Step B — Phase 0 Safety → 100% done

- Heap floor / RAM assert / status heap fields
- OTA preflight script
- Postgres backup before wipe
- Unknown MQTT tags → `pending_devices`

### Step C — Milestone M1 → effectively 100% of planned M1 scope done

#### Wave 1 — Foundations (done)

- Vendor type, sites CRUD, device models
- Device lifecycle + model/site/comms
- Birth/announce MQTT + pending enrich
- Health → `device_health_latest`
- Vendor-type landing copy
- Seed demo vendor + site

#### Wave 2 — Capabilities (done)

- Capability/metric registry (V9)
- Handler pipeline (`WaterLevelHandler`, `EnvHandler`, `SoilMoistureHandler`)
- Water still writes `level_readings` (no rename)
- `GET /api/capabilities`

#### Wave 3 — Topology (done)

- Assets / flows / bindings (V10)
- Provision API + registration wizard UI (`/setup`)

### Step D — Milestone M2 Control → 0% (not started)

Pending by design:

- Fault/interlock taxonomy
- Control policy engine (`CLOUD|EDGE`)
- Multi-pump coordination
- Stale-input fail-safe
- Firmware motor interlocks / 3-phase
- Command audit / maintenance lockout

### Step E — Milestone M3 Scale → 0% (not started; LoRa parked)

- Transport abstraction, LoRa/4G ingress, edge autonomy, dry-run, predictive analytics

---

## 5. How complete is the whole redesign?

| Block | Share of full vision | Status |
|--------|----------------------|--------|
| Decisions + Safety + M1 | ~40–45% | **Done** |
| M2 Control | ~35–40% | **Pending** |
| M3 Scale | ~15–20% | **Pending / parked pieces** |

Summary:

- **Of the agreed first release milestone (M1): complete.**
- **Of the full long-term redesign: foundation half largely done; control half still ahead.**
- **Of “usable water product today”:** dashboard + MQTT level path still work; new APIs/wizard exist; **auto-pump intelligence (M2) is not built yet.**

---

## 6. What is pending (clear list)

### Must do next (M2) — smart water control

1. Define shared fault/interlock codes  
2. Build control policy model + engine  
3. Multi-pump / coordination groups  
4. Fail-safe when sensor data is stale  
5. Firmware hard interlocks (relay / later 3-phase)  
6. Override + maintenance lockout in UI  

### Should do continuously

- Automated tests for ingest + (later) policy  
- Keep this status file and story checkboxes aligned with reality  
- Keep API + MQTT + Postgres running for local verification  

### Explicitly out of scope for now

- LoRa design  
- SudarshanChakra track  
- Renaming / partition surgery of `level_readings`  

### Local clutter (not product backlog)

Uncommitted local/runtime files such as `data/config.json`, CA pem, MQTT lock dirs, bundles — intentionally not part of the M1 commit.

---

## 7. Plan and approach to finish

```text
1. Stabilize M1
   - Start API + MQTT + Postgres
   - Smoke: login → dashboard level → pending announce → /setup Home topology
   - Keep docs aligned with code

2. Milestone M2 (sequential, doable)
   P.1 taxonomy → P.2 policy engine → P.3/P.4 coordination & fail-safe
   → P.7/P.8 audit/override → P.5 firmware relay interlocks → P.6 3-phase later

3. Milestone M3 (only when needed)
   WiFi/4G first; LoRa only when hardware exists
```

### Approach principles (locked)

- Additive migrations only  
- Keep water dashboard working every step  
- Soft intent in cloud; hard safety in firmware  
- Topology first, then policies that reason about **assets**, not raw devices  

---

## 8. Is it doable?

**Yes.**

Reasons:

1. Hard architecture decisions are already locked.  
2. M1 — the risky “big start” schema/API/wizard slice — is already in git and DB.  
3. Remaining M2 work is large but well-scoped in the stories doc; it builds on tables that already exist (`coordination_group` stub, bindings, capabilities).  
4. M3 is optional/parked for LoRa; not blocking a useful M2 product.  

Caveats:

- M2 is the hardest product slice (safety + automation).  
- Planning must use **code + Flyway + this status file**, not stale ☐ marks alone.  
- Full live end-to-end smoke must be re-run whenever the API has been stopped for a while.

---

## 9. Verification / “is status up to date?”

| Item | Up to date? |
|------|-------------|
| Git commit on remote | **Yes** — M1 pushed (`9295dc2`) |
| Migrations on disk (V7–V10) | **Yes** |
| Migrations applied in local Postgres | **Yes** (when cluster is running with prior M1 apply) |
| Source files for handlers/wizard/topology | **Yes** |
| Story checklist ☑/☐ in `BACKEND-REDESIGN-STORIES.md` | **May be stale** |
| Running API/smoke at any given moment | **Check locally** — start services if health is down |

**Verdict:**

- **Implementation status is up to date in git/DB.**  
- **Use this file as the human-readable completion status.**  
- **Re-smoke runtime before claiming a demo is live.**

---

## 10. Bottom line

We finished the **platform foundation (M1)**.  
We have not started **smart control (M2)**.  

The aim is clear, the path is doable, and the next real work is the **control-policy milestone** — after services are up and smoke checks pass.

---

## 11. How to redeploy remote host + burn firmware

**Full runbook (step-by-step):** [DEPLOY-AND-OTA.md](DEPLOY-AND-OTA.md)

### Remote VPS (platform) — short path

```bash
cd server
# First time only:
#   cp config/remote.env.example config/remote.env && chmod 600 config/remote.env
#   edit host/user/password/path → ./flm-server.sh remote-install

# After pulling M1 / bugfix commits (keeps DB + .env):
./flm-server.sh remote-update
# or: remote-update-backend | remote-update-frontend
```

Smoke: `http://YOUR_HOST/` and `curl -sf http://YOUR_HOST:8080/actuator/health`.

### Firmware (ESP8266) — short path

```bash
# USB (reliable)
pio run -e sensor_only_d1mini -t upload
pio run -e sensor_only_d1mini -t uploadfs

# OTA (same LAN; set --host_ip in platformio.ini; allow UFW reverse TCP)
scripts/ota_preflight.sh <device-ip> upload
```

Or use the device web **Firmware** tab to upload `firmware.bin`.

After flash: point MQTT at the VPS; announce appears under pending / wizard (`/setup`).
