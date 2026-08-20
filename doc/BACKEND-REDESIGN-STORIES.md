# FLM Redesign — Implementable Stories

> Backlog derived from `doc/BACKEND-REDESIGN.md`. Each story is scoped to a single focused
> agent session: clear outcome, acceptance criteria, dependencies, and files touched.
> Track: **BE** = backend (`server/backend`), **FE** = frontend (`server/frontend`),
> **FW** = firmware (`src/`), **INFRA** = scripts/ops, **DOC** = docs.
>
> Status legend: ☐ todo · ◐ in progress · ☑ done. `V7` migration already drafted.

---

## Story ID scheme

`FLM-<epic>.<n>` — epics:
- **D** Decisions (blocking)
- **S** Safety net (Phase 0)
- **F** Foundations (Phase 1)
- **C** Capabilities (Phase 2)
- **L** Logical layer / topology (Phase 3)
- **P** Control & policy (Phase 4)
- **T** Transport & scale (Phase 5)
- **X** Cross-cutting (tests, docs, seed)

Dependencies are listed per story. Do not start a story until its deps are ☑.

---

## EPIC D — Decisions (blocking, no code)

### FLM-D.1 — Confirm target backend track (A vs B) ☑ (2026-07-17)
- **RESOLVED: Track B.** SudarshanChakra is not in place; deferred to much later. All work
  targets the standalone `server/` platform. `CONSOLIDATED_PLAN.md` = future option only.
- Recorded in `doc/BACKEND-REDESIGN.md §20`.
- **Blocks:** (now unblocked) all F/C/L/P/T stories.

### FLM-D.2 — Confirm client track (SC Android vs `server/frontend`) ☑ (2026-07-17)
- **RESOLVED: `server/frontend`** (React). No SC Android app in scope now.
- **Deps:** FLM-D.1. **Blocks:** (now unblocked) all FE stories.

### FLM-D.3 — Lock key architecture decisions ☑ (2026-07-17)
- **RESOLVED** (see `doc/BACKEND-REDESIGN.md §18`):
  1. Telemetry storage → **HYBRID**
  2. Rule execution → **BOTH**, per-policy `execution = CLOUD | EDGE` field, vendor-selectable
  3. LoRa → **PARKED**
  4. Topology layer → **YES** (commit)
  5. Two-tier control → **YES** (confirmed)
  6. Protection profile → **DATA** (catalog JSONB)
  7. First increment → **BIG START** (Foundations + Capabilities + Topology as one milestone)
- **Blocks:** (now unblocked) C/L/P/T epics.
- **Impact:** #2 adds an `execution` field to `control_policy` (FLM-P.2). #7 changes release
  sequencing — see "Suggested execution order" below.

---

## EPIC S — Safety net (Phase 0) — ☑ complete (2026-07-17)

### FLM-S.1 — Firmware heap floor guard ☑  (FW)
- **Do:** in `src/main.cpp processLoop()`, if heap below `FLM_HEAP_FLOOR_BYTES` (8 KB),
  skip MQTT publish and log a throttled warning (`HeapMonitor`).
- **Files:** `src/utils/HeapMonitor.h`, `src/main.cpp`.
- **Acceptance:** low-heap path skips publish; builds `pio run -e sensor_only_d1mini`.
- **Deps:** none.

### FLM-S.2 — Compile-time RAM budget assert ☑  (FW)
- **Do:** `static_assert` on `sizeof(MqttLogEntry) * MQTT_LOG_CAPACITY <= 4096`.
- **Files:** `src/network/MQTTManager.h`.
- **Acceptance:** build fails if capacity pushed wastefully; passes at 24.
- **Deps:** none.

### FLM-S.3 — Expose min-heap + max-free-block in `/api/status` ☑  (FW)
- **Do:** add `minFreeHeap`, `maxFreeBlock` to status JSON.
- **Files:** `src/network/WebServerApi.cpp`, `src/utils/HeapMonitor.h`.
- **Acceptance:** `/api/status` shows the new fields.
- **Deps:** none. **Enables:** FLM-F.7 (health ingest).

### FLM-S.4 — OTA pre-flight check script ☑  (INFRA)
- **Do:** ping + `/api/info` + host IP / UFW hint, then optional upload/uploadfs.
- **Files:** `scripts/ota_preflight.sh`.
- **Acceptance:** fails with specific check reason when device unreachable.
- **Deps:** none.

### FLM-S.5 — `pg_dump` backup helper before DB wipe ☑  (INFRA)
- **Do:** `postgres_backup()` → `server/backups/*.sql.gz`; called at start of uninstall;
  skip with `FLM_SKIP_BACKUP=1`.
- **Files:** `server/scripts/lib/postgres.sh`, `.gitignore`.
- **Acceptance:** uninstall attempts a dump first.
- **Deps:** none.

### FLM-S.6 — Unregistered-device "pending" queue ☑  (BE)
- **Do:** `pending_devices` table (Flyway **V8**); upsert unknown MQTT tags; `GET /api/devices/pending`.
- **Files:** `V8__pending_devices.sql`, `PendingDeviceDao`, `MqttIngestService`, `DeviceService`, `DeviceController`.
- **Acceptance:** unknown device appears in pending list; no level_readings row until registered.
- **Deps:** none. **Relates:** FLM-F.6 (self-registration).

---

## EPIC F — Foundations (Phase 1)

### FLM-F.1 — Apply `V7` migration ☑-drafted → ☐ apply  (BE)
- **Do:** apply `V7__generic_node_foundations.sql` (restart `flm-api` so Flyway runs it).
- **Acceptance:** `flyway_schema_history` shows v7; `vendors.vendor_type`, `sites`,
  `device_model` (3 seeded rows), device columns, `device_health_latest`, `lifecycle_event`
  all present. API health UP.
- **Deps:** FLM-D.1.

### FLM-F.2 — Vendor type entity + API ☐  (BE)
- **Do:** add `vendorType` to `Vendor` entity + `VendorType` enum in `flm-common`; expose in
  vendor DTO/endpoints; validation against the 5 allowed values.
- **Files:** `Vendor.java`, `flm-common`, vendor controller/service/DTO.
- **Acceptance:** create/read vendor returns/accepts `vendorType`; invalid value rejected.
- **Deps:** FLM-F.1.

### FLM-F.3 — Device model catalog entity + read API ☐  (BE)
- **Do:** `DeviceModel` entity + repository; `GET /api/device-models` (list/detail).
- **Files:** `flm-domain`, `flm-api`.
- **Acceptance:** endpoint lists the 3 seeded models with role/comms/profiles.
- **Deps:** FLM-F.1.

### FLM-F.4 — Site entity + CRUD API (tenant-scoped) ☐  (BE)
- **Do:** `Site` entity + repo + `SiteService` (uses `TenantGuard`) + CRUD endpoints.
- **Files:** `flm-domain`, `flm-api`.
- **Acceptance:** a vendor can CRUD only their sites; super-admin sees all; geo fields
  round-trip.
- **Deps:** FLM-F.1.

### FLM-F.5 — Device lifecycle + model/site/comms fields ☐  (BE)
- **Do:** map new `Device` columns (`modelId`, `siteId`, `commType`, `lifecycleState`,
  `attributes`) in entity/DTO; add `POST /api/devices/{id}/lifecycle` that validates
  transitions and writes `lifecycle_event`.
- **Files:** `Device.java`, `DeviceService.java`, `DeviceController.java`, migration if a
  transition table is needed.
- **Acceptance:** illegal transition (e.g. DECOMMISSIONED→ACTIVE) rejected; valid one
  audited; register defaults to `PROVISIONED` or `ACTIVE` per decision.
- **Deps:** FLM-F.1, FLM-F.3, FLM-F.4.

### FLM-F.6 — Device self-registration (birth/announce) ☐  (BE + FW)
- **FW:** publish a retained birth message on connect: `{tag}/system/announce`
  `{model_key, chip_id, firmware, comm, capabilities[]}`.
- **BE:** ingest announce → upsert `pending_devices` or auto-provision; link `model_id`.
- **Files:** `src/network/MQTTManager.cpp`, `WiFiManager.cpp`; `flm-mqtt-bridge`.
- **Acceptance:** a fresh device shows up with its model + capabilities without manual entry.
- **Deps:** FLM-F.3, FLM-S.6.

### FLM-F.7 — Device health ingest + snapshot API ☐  (BE)
- **Do:** parse `{tag}/system/health` (or fields from status) into `device_health_latest`;
  expose in device summary/detail.
- **Files:** `flm-mqtt-bridge`, `flm-api` device DTO.
- **Acceptance:** `device_health_latest` populated; `GET /api/devices/{id}` shows heap/rssi/
  ip/mac/uptime.
- **Deps:** FLM-F.1, FLM-S.3 (firmware fields).

### FLM-F.8 — Vendor-type dashboard profile (FE) ☐  (FE)
- **Do:** login lands on a profile-specific view per `vendorType` (House / Building /
  Network / Field); config-driven, not hardcoded per screen.
- **Files:** `server/frontend`.
- **Acceptance:** switching a test vendor's type changes the landing dashboard.
- **Deps:** FLM-D.2, FLM-F.2.

---

## EPIC C — Capabilities (Phase 2)

### FLM-C.1 — `V9` capability + metric registry migration ☐  (BE)
- **Do:** `capabilities`, `metrics` (key, unit, data_type, min/max, display),
  `node_capabilities` (channels: node_id, capability_id, chan_index, name, attributes).
  (Note: `V8` is reserved for `pending_devices` from Phase 0 / FLM-S.6.)
- **Acceptance:** migration applies; seed core capabilities (water_level, env, soil_moisture,
  motor, valve, motor_electrical, health) + their metrics.
- **Deps:** FLM-D.3, FLM-F.1.

### FLM-C.2 — `CapabilityHandler` plugin pipeline ☐  (BE)
- **Do:** define `CapabilityHandler` interface (`parse/validate/persist/commands/metrics`);
  refactor ingest to `transport → normalize → route by capability → handler`.
- **Files:** `flm-mqtt-bridge`.
- **Acceptance:** ingest routes by capability; unknown capability handled gracefully; unit
  test for routing.
- **Deps:** FLM-C.1.

### FLM-C.3 — Migrate water level to `measure.water_level` handler ☐  (BE)
- **Do:** implement `WaterLevelHandler`; map `level_readings`→`telemetry_water` (or alias);
  ensure existing dashboard/queries keep working.
- **Acceptance:** water telemetry flows through the handler; no regression in current UI/API.
- **Deps:** FLM-C.2.

### FLM-C.4 — Add env + soil-moisture handlers ☐  (BE)
- **Do:** `EnvHandler` (temp/humidity) and `SoilMoistureHandler` + `telemetry_env`,
  `telemetry_soil` tables (partitioned per V6 pattern).
- **Acceptance:** simulated env/soil payloads persist + appear via API.
- **Deps:** FLM-C.2, FLM-C.1.

### FLM-C.5 — Registry-driven metric validation + UI hints ☐  (BE + FE)
- **Do:** validate incoming metrics against registry (unit/range); expose registry so FE can
  render gauges without code changes.
- **Acceptance:** out-of-range value flagged; a new metric row renders in UI automatically.
- **Deps:** FLM-C.1, FLM-C.2.

---

## EPIC L — Logical layer / topology (Phase 3)

### FLM-L.1 — `V10` asset/topology migration ☐  (BE)
- **Do:** `asset`, `flow_edge`, `binding`, `coordination_group`, `node_relationships`.
- **Acceptance:** migration applies; FK integrity; can model Home + Farm by hand-inserting.
- **Deps:** FLM-D.3, FLM-C.1.

### FLM-L.2 — Asset + flow CRUD API (tenant/site scoped) ☐  (BE)
- **Do:** services + endpoints for assets and flow edges within a site.
- **Acceptance:** build the Home topology (municipal→sump→pump→overhead) via API;
  tenant-scoped.
- **Deps:** FLM-L.1, FLM-F.4.

### FLM-L.3 — Channel↔asset binding API ☐  (BE)
- **Do:** bind a `node_capability` (channel) to an asset with role MEASURES/ACTUATES;
  time-bounded (supports sensor swap).
- **Acceptance:** rebinding a replaced sensor preserves asset history; overlapping active
  bindings rejected.
- **Deps:** FLM-L.1, FLM-C.1.

### FLM-L.4 — Registration wizard API (orchestration) ☐  (BE)
- **Do:** endpoints backing the guided flow: create site → declare assets → draw flows →
  add devices from catalog → bind → (policies later).
- **Acceptance:** a scripted end-to-end call sequence provisions a full Home site.
- **Deps:** FLM-L.2, FLM-L.3, FLM-F.3, FLM-F.5.

### FLM-L.5 — Registration wizard UI ☐  (FE)
- **Do:** multi-step wizard driven by `vendorType`; offers only catalog-compatible bindings.
- **Acceptance:** a non-technical flow provisions Home + Farm topologies.
- **Deps:** FLM-D.2, FLM-L.4.

---

## EPIC P — Control & policy (Phase 4)

### FLM-P.1 — Fault/interlock code taxonomy (shared) ☐  (BE + FW + DOC)
- **Do:** a single enum/list (`phase_missing`, `phase_reversed`, `single_phasing`, …,
  `sensor_stale`, `comms_lost`) with severity + latching/auto-reset + detecting tier; shared
  by firmware, backend, UI.
- **Files:** `flm-common`, firmware header, `doc/BACKEND-REDESIGN.md §16`.
- **Acceptance:** one source of truth referenced by all three layers.
- **Deps:** FLM-D.3.

### FLM-P.2 — Control policy model + engine (soft intent) ☐  (BE)
- **Do:** `control_policy` + `control_condition` tables; engine evaluates on telemetry
  ingest; multi-input conditions, hysteresis, publishes commands to actuator assets.
- **Acceptance:** "Pump ON when OHT<20% AND Sump>15%" fires correctly against simulated
  levels; hysteresis prevents chatter.
- **Deps:** FLM-L.2, FLM-L.3, FLM-C.2.

### FLM-P.3 — Multi-pump coordination ☐  (BE)
- **Do:** `coordination_group` strategies LEAD_LAG / ALTERNATE / FAILOVER; "don't fill two
  linked tanks at once" rule.
- **Acceptance:** two pumps on one tank alternate; failover engages when lead pump fails to
  raise level within confirm timeout.
- **Deps:** FLM-P.2.

### FLM-P.4 — Stale-input & fail-safe handling ☐  (BE)
- **Do:** if a measure device feeding a policy goes offline/stale, policy fails safe (hold /
  time fallback / alert), never runs blind.
- **Acceptance:** simulated sensor dropout → pump not commanded; alert raised.
- **Deps:** FLM-P.2, FLM-F.7.

### FLM-P.5 — Firmware single-phase motor interlocks ☐  (FW)
- **Do:** relay controller enforces voltage/current/dry-run/cooldown/max-runtime; reports
  refusals with fault codes.
- **Files:** `src/motor/RelayMotorController.cpp`.
- **Acceptance:** motor refuses ON under dry-run; publishes `{tag}/motor/alert` with a fault
  code from FLM-P.1.
- **Deps:** FLM-P.1.

### FLM-P.6 — Three-phase controller state machine (firmware spec + impl) ☐  (FW)
- **Do:** implement the start/run/trip state machine (phase presence, R-Y-B sequence,
  single-phasing, imbalance) with latching vs auto-reset per FLM-P.1; commissioning
  phase-sequence capture.
- **Files:** new `src/motor/ThreePhaseMotorController.cpp`, catalog model `motor_3ph`.
- **Acceptance:** simulated phase reversal → latched refusal; missing phase → blocked start;
  single-phasing while running → trip + cooldown.
- **Deps:** FLM-P.1, FLM-P.5.

### FLM-P.7 — Command confirmation + audit ☐  (BE)
- **Do:** commanded ON → expect level rise/current within N; else fault. Log all control
  actions + firing condition to `platform_audit_log` + `interlock_event`.
- **Acceptance:** audit shows who/what/why for every pump action.
- **Deps:** FLM-P.2.

### FLM-P.8 — Manual override & MAINTENANCE lockout ☐  (BE + FE)
- **Do:** user force ON/OFF; MAINTENANCE lifecycle state blocks automation + real actuation.
- **Acceptance:** device in MAINTENANCE ignores policy commands; override audited.
- **Deps:** FLM-P.2, FLM-F.5.

---

## EPIC T — Transport & scale (Phase 5)

### FLM-T.1 — Transport abstraction (DeviceEvent normalization) ☐  (BE)
- **Do:** normalize all inbound (MQTT now) to an internal `DeviceEvent` before domain;
  generalize topic scheme to `{tag}/{capability}/{stream}`.
- **Acceptance:** domain/handlers no longer reference MQTT directly; existing flow intact.
- **Deps:** FLM-C.2.

### FLM-T.2 — LoRa/4G ingress ☐  (BE + INFRA)
- **Do:** LoRa (via gateway/network server) → HTTP/MQTT bridge into the same ingest;
  `comm_type` respected.
- **Acceptance:** a simulated LoRa payload lands as a `DeviceEvent` and persists.
- **Deps:** FLM-T.1, FLM-D.3 (LoRa flavor).

### FLM-T.3 — Edge gateway policy autonomy ☐  (BE/edge)
- **Do:** push a policy definition to an edge gateway so control survives backhaul loss; same
  definition, edge execution.
- **Acceptance:** with cloud unreachable, edge keeps a pump loop running safely.
- **Deps:** FLM-P.2, FLM-T.2.

### FLM-T.4 — Simulation / dry-run mode ☐  (BE)
- **Do:** replay historical telemetry against a new topology + policy without actuating.
- **Acceptance:** a policy can be validated on past data; no commands published.
- **Deps:** FLM-P.2.

### FLM-T.5 — Predictive phase analytics ☐  (BE)
- **Do:** trend per-phase V/I to flag imbalance before failure; energy/cost reporting.
- **Acceptance:** a rising-imbalance dataset produces a predictive alert.
- **Deps:** FLM-C.4-style motor_electrical telemetry, FLM-P.6.

---

## EPIC X — Cross-cutting

### FLM-X.1 — Seed/bootstrap data ☐  (BE)
- **Do:** idempotent seed: default vendor + admin user + example site so a fresh DB is
  usable immediately (fixes today's "empty DB, hand-create everything").
- **Acceptance:** fresh `flmDB` + backend start → can log in and see an example site.
- **Deps:** FLM-F.2, FLM-F.4.

### FLM-X.2 — Fold reliability/OTA learnings into `TROUBLESHOOTING.md` ☑  (DOC)
- **Do:** document the OOM-reboot cause, heap budget, and OTA firewall return-path fix.
- **Acceptance:** troubleshooting doc has both incidents with resolutions.
- **Deps:** none.

### FLM-X.3 — Test harness for ingest + policy ☐  (BE)
- **Do:** MQTT/HTTP payload fixtures + tests for handlers, policy engine, coordination,
  fail-safe.
- **Acceptance:** CI-runnable tests cover the core ingest→persist→command path.
- **Deps:** FLM-C.2, FLM-P.2.

---

## Suggested execution order

> Decisions D.1–D.3 are ☑ resolved. #7 = **BIG START**: Foundations + Capabilities +
> Topology are treated as **one milestone (M1)**, built together before the first release,
> rather than shipped incrementally.

1. **Now / parallel-safe:** FLM-S.1–S.6, FLM-X.2 (no deps, high value).
2. **Milestone M1 (big start — Foundations + Capabilities + Topology):**
   - Foundations: FLM-F.1 → F.2/F.3/F.4 → F.5 → F.6/F.7 → F.8; FLM-X.1 (seed).
   - Capabilities: FLM-C.1 → C.2 → C.3 → C.4 → C.5.
   - Topology: FLM-L.1 → L.2/L.3 → L.4 → L.5.
   - (Foundations must precede Capabilities must precede Topology, but M1 ships as a whole.)
3. **Milestone M2 (control):** FLM-P.1 → P.2 (incl. per-policy `execution` field) →
   P.3/P.4/P.7/P.8 → P.5 → P.6.
4. **Milestone M3 (scale):** FLM-T.1 → T.2 → T.3/T.4/T.5 (LoRa parked until unparked).
5. **Continuous:** FLM-X.3 (tests) alongside every milestone.
