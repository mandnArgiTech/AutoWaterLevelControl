-- V7: Foundations for the generic IoT-node platform (see doc/BACKEND-REDESIGN.md).
-- Phase 1 increment — ALL ADDITIVE, backward compatible with the water-only model.
--   * vendor_type            → drives dashboard profile + enabled capabilities
--   * device_model catalog   → device types as data, not enums (sensor/motor_relay/motor_sms/...)
--   * device lifecycle/comms → lifecycle state machine + transport + free-form attributes
--   * sites                  → installation grouping (home / farm / village / field)
--   * device_health_latest   → node performance/health snapshot (heap, rssi, ip, cpu)
-- Existing water level flow is untouched; it simply becomes one capability among many later.

-- ---------------------------------------------------------------------------
-- 1. Vendor type (solution profile)
-- ---------------------------------------------------------------------------
ALTER TABLE vendors
    ADD COLUMN IF NOT EXISTS vendor_type VARCHAR(32) NOT NULL DEFAULT 'HOUSEHOLD';

DO $$
BEGIN
    IF NOT EXISTS (
        SELECT 1 FROM pg_constraint WHERE conname = 'chk_vendors_vendor_type'
    ) THEN
        ALTER TABLE vendors ADD CONSTRAINT chk_vendors_vendor_type
            CHECK (vendor_type IN ('HOUSEHOLD','APARTMENT','WATER_UTILITY','IRRIGATION','OTHER'));
    END IF;
END $$;

-- ---------------------------------------------------------------------------
-- 2. Sites (installation grouping)
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS sites (
    id           UUID PRIMARY KEY,
    vendor_id    UUID NOT NULL REFERENCES vendors(id),
    name         VARCHAR(255) NOT NULL,
    kind         VARCHAR(32)  NOT NULL DEFAULT 'GENERIC',   -- HOME | FARM | VILLAGE | FIELD | GENERIC
    latitude     DOUBLE PRECISION,
    longitude    DOUBLE PRECISION,
    address      VARCHAR(512),
    attributes   JSONB,
    active       BOOLEAN NOT NULL DEFAULT TRUE,
    created_at   TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_sites_vendor ON sites(vendor_id);

-- ---------------------------------------------------------------------------
-- 3. Device model catalog (device types as data)
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS device_model (
    id                    UUID PRIMARY KEY,
    model_key             VARCHAR(64) NOT NULL UNIQUE,       -- e.g. 'sensor', 'motor_relay', 'motor_sms', 'motor_3ph'
    name                  VARCHAR(128) NOT NULL,
    role                  VARCHAR(16)  NOT NULL              -- MEASURE | CONTROL | BOTH | GATEWAY
        CHECK (role IN ('MEASURE','CONTROL','BOTH','GATEWAY')),
    board                 VARCHAR(64),                       -- e.g. 'd1_mini', 'esp32'
    mcu                   VARCHAR(32),                       -- e.g. 'ESP8266', 'ESP32'
    default_comm          VARCHAR(16) NOT NULL DEFAULT 'wifi'-- wifi | cellular_4g | lora
        CHECK (default_comm IN ('wifi','cellular_4g','lora')),
    -- Data-driven profiles (validated in app against the metric/capability registry, added later)
    capability_profile    JSONB,                             -- ["measure.water_level","system.health",...]
    protection_profile    JSONB,                             -- ["phase_missing","phase_reversed",...] for controllers
    -- Where hard electrical safety interlocks are enforced for CONTROL models:
    --   'firmware'  = our node enforces (relay / 3-phase controllers we build)
    --   'external'  = an external panel enforces (e.g. Taro Smart Panel over SMS)
    --   'none'      = not applicable (measure-only)
    interlock_tier        VARCHAR(16) NOT NULL DEFAULT 'none'
        CHECK (interlock_tier IN ('firmware','external','none')),
    created_at            TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Seed the models that already exist as firmware roles (platformio.ini FLM_ROLE_*).
INSERT INTO device_model (id, model_key, name, role, board, mcu, default_comm,
                          capability_profile, protection_profile, interlock_tier)
VALUES
    (gen_random_uuid(), 'sensor', 'Level / environment sensor', 'MEASURE',
     'd1_mini', 'ESP8266', 'wifi',
     '["measure.water_level","measure.env","system.health"]'::jsonb, NULL, 'none'),
    (gen_random_uuid(), 'motor_relay', 'Relay motor controller', 'CONTROL',
     'd1_mini', 'ESP8266', 'wifi',
     '["control.motor","system.health"]'::jsonb,
     '["dry_run","over_current","max_runtime"]'::jsonb, 'firmware'),
    (gen_random_uuid(), 'motor_sms', 'SMS motor controller (external panel)', 'CONTROL',
     'd1_mini', 'ESP8266', 'cellular_4g',
     '["control.motor","system.health"]'::jsonb,
     NULL, 'external')
ON CONFLICT (model_key) DO NOTHING;

-- ---------------------------------------------------------------------------
-- 4. Device: model link, lifecycle, transport, attributes, site
-- ---------------------------------------------------------------------------
ALTER TABLE devices
    ADD COLUMN IF NOT EXISTS model_id        UUID REFERENCES device_model(id),
    ADD COLUMN IF NOT EXISTS site_id         UUID REFERENCES sites(id),
    ADD COLUMN IF NOT EXISTS comm_type       VARCHAR(16) NOT NULL DEFAULT 'wifi',
    ADD COLUMN IF NOT EXISTS lifecycle_state VARCHAR(16) NOT NULL DEFAULT 'ACTIVE',
    ADD COLUMN IF NOT EXISTS attributes      JSONB;

DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_constraint WHERE conname = 'chk_devices_comm_type') THEN
        ALTER TABLE devices ADD CONSTRAINT chk_devices_comm_type
            CHECK (comm_type IN ('wifi','cellular_4g','lora'));
    END IF;
    IF NOT EXISTS (SELECT 1 FROM pg_constraint WHERE conname = 'chk_devices_lifecycle_state') THEN
        ALTER TABLE devices ADD CONSTRAINT chk_devices_lifecycle_state
            CHECK (lifecycle_state IN
                ('PROVISIONED','INSTALLED','ACTIVE','MAINTENANCE','FAULT','DECOMMISSIONED'));
    END IF;
END $$;

CREATE INDEX IF NOT EXISTS idx_devices_site  ON devices(site_id);
CREATE INDEX IF NOT EXISTS idx_devices_model ON devices(model_id);

-- Lifecycle transition audit (who/what moved a device between states, and why).
CREATE TABLE IF NOT EXISTS lifecycle_event (
    id          UUID PRIMARY KEY,
    device_id   UUID NOT NULL REFERENCES devices(id),
    from_state  VARCHAR(16),
    to_state    VARCHAR(16) NOT NULL,
    reason      VARCHAR(255),
    actor       VARCHAR(128),
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_lifecycle_event_device
    ON lifecycle_event(device_id, created_at DESC);

-- ---------------------------------------------------------------------------
-- 5. Device health snapshot (node performance / network metrics)
--    Latest-only for now; historical health telemetry deferred to the
--    per-capability telemetry phase (see doc §15, V9).
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS device_health_latest (
    device_id        UUID PRIMARY KEY REFERENCES devices(id),
    received_at      TIMESTAMPTZ NOT NULL,
    free_heap        INTEGER,     -- bytes
    min_free_heap    INTEGER,     -- bytes (low-water mark)
    max_free_block   INTEGER,     -- bytes (fragmentation indicator)
    cpu_percent      SMALLINT,
    rssi             SMALLINT,     -- dBm
    ip_address       VARCHAR(45),  -- IPv4/IPv6
    mac_address      VARCHAR(17),
    uptime_ms        BIGINT,
    firmware         VARCHAR(64)
);
