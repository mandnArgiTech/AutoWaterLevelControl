-- V9: Capability + metric registry, node channels, env/soil telemetry (FLM-C.1 … C.4).

-- ---------------------------------------------------------------------------
-- Capability registry
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS capabilities (
    id          UUID PRIMARY KEY,
    cap_key     VARCHAR(64) NOT NULL UNIQUE,   -- e.g. measure.water_level
    name        VARCHAR(128) NOT NULL,
    category    VARCHAR(32) NOT NULL DEFAULT 'measure',  -- measure | control | system
    description VARCHAR(512),
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS metrics (
    id              UUID PRIMARY KEY,
    capability_id   UUID NOT NULL REFERENCES capabilities(id) ON DELETE CASCADE,
    metric_key      VARCHAR(64) NOT NULL,
    name            VARCHAR(128) NOT NULL,
    unit            VARCHAR(32),
    data_type       VARCHAR(16) NOT NULL DEFAULT 'float',  -- float | int | bool | string
    min_value       DOUBLE PRECISION,
    max_value       DOUBLE PRECISION,
    display_hint    VARCHAR(64),
    UNIQUE (capability_id, metric_key)
);

CREATE TABLE IF NOT EXISTS node_capabilities (
    id              UUID PRIMARY KEY,
    device_id       UUID NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    capability_id   UUID NOT NULL REFERENCES capabilities(id),
    chan_index      SMALLINT NOT NULL DEFAULT 0,
    name            VARCHAR(128),
    attributes      JSONB,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (device_id, capability_id, chan_index)
);

CREATE INDEX IF NOT EXISTS idx_node_capabilities_device ON node_capabilities(device_id);
CREATE INDEX IF NOT EXISTS idx_metrics_capability ON metrics(capability_id);

-- Seed core capabilities
INSERT INTO capabilities (id, cap_key, name, category, description)
VALUES
    ('a1000000-0000-4000-8000-000000000001', 'measure.water_level', 'Water level', 'measure', 'Tank / reservoir fill level'),
    ('a1000000-0000-4000-8000-000000000002', 'measure.env', 'Environment', 'measure', 'Temperature and humidity'),
    ('a1000000-0000-4000-8000-000000000003', 'measure.soil_moisture', 'Soil moisture', 'measure', 'Soil moisture percent'),
    ('a1000000-0000-4000-8000-000000000004', 'control.motor', 'Motor control', 'control', 'Pump / motor actuator'),
    ('a1000000-0000-4000-8000-000000000005', 'control.valve', 'Valve control', 'control', 'Valve actuator'),
    ('a1000000-0000-4000-8000-000000000006', 'measure.motor_electrical', 'Motor electrical', 'measure', 'Current / voltage / power'),
    ('a1000000-0000-4000-8000-000000000007', 'system.health', 'System health', 'system', 'Node heap, RSSI, uptime')
ON CONFLICT (cap_key) DO NOTHING;

INSERT INTO metrics (id, capability_id, metric_key, name, unit, data_type, min_value, max_value, display_hint)
VALUES
    (gen_random_uuid(), 'a1000000-0000-4000-8000-000000000001', 'percent_filled', 'Percent filled', '%', 'float', 0, 100, 'gauge'),
    (gen_random_uuid(), 'a1000000-0000-4000-8000-000000000001', 'volume_liters', 'Volume', 'L', 'float', 0, NULL, 'number'),
    (gen_random_uuid(), 'a1000000-0000-4000-8000-000000000001', 'water_height_mm', 'Water height', 'mm', 'int', 0, NULL, 'number'),
    (gen_random_uuid(), 'a1000000-0000-4000-8000-000000000001', 'distance_mm', 'Sensor distance', 'mm', 'int', 0, NULL, 'number'),
    (gen_random_uuid(), 'a1000000-0000-4000-8000-000000000002', 'temperature_c', 'Temperature', '°C', 'float', -40, 85, 'gauge'),
    (gen_random_uuid(), 'a1000000-0000-4000-8000-000000000002', 'humidity_pct', 'Humidity', '%', 'int', 0, 100, 'gauge'),
    (gen_random_uuid(), 'a1000000-0000-4000-8000-000000000003', 'moisture_pct', 'Soil moisture', '%', 'float', 0, 100, 'gauge'),
    (gen_random_uuid(), 'a1000000-0000-4000-8000-000000000006', 'current_a', 'Current', 'A', 'float', 0, NULL, 'number'),
    (gen_random_uuid(), 'a1000000-0000-4000-8000-000000000006', 'voltage_v', 'Voltage', 'V', 'float', 0, NULL, 'number'),
    (gen_random_uuid(), 'a1000000-0000-4000-8000-000000000007', 'free_heap', 'Free heap', 'B', 'int', 0, NULL, 'number'),
    (gen_random_uuid(), 'a1000000-0000-4000-8000-000000000007', 'rssi', 'Wi‑Fi RSSI', 'dBm', 'int', -100, 0, 'number')
ON CONFLICT DO NOTHING;

-- ---------------------------------------------------------------------------
-- Env + soil partitioned telemetry (water stays on level_readings)
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS telemetry_env (
    device_id       UUID NOT NULL,
    received_at     TIMESTAMPTZ NOT NULL,
    temperature_c   REAL,
    humidity_pct    SMALLINT,
    PRIMARY KEY (device_id, received_at)
) PARTITION BY RANGE (received_at);

CREATE TABLE IF NOT EXISTS telemetry_soil (
    device_id       UUID NOT NULL,
    received_at     TIMESTAMPTZ NOT NULL,
    moisture_pct    REAL,
    PRIMARY KEY (device_id, received_at)
) PARTITION BY RANGE (received_at);

CREATE OR REPLACE FUNCTION create_telemetry_env_partition(p_year INT, p_month INT)
RETURNS VOID LANGUAGE plpgsql AS $$
DECLARE
    start_ts TIMESTAMPTZ;
    end_ts   TIMESTAMPTZ;
    part_name TEXT;
BEGIN
    start_ts := make_timestamptz(p_year, p_month, 1, 0, 0, 0, 'UTC');
    end_ts   := start_ts + INTERVAL '1 month';
    part_name := format('telemetry_env_%s', to_char(start_ts, 'YYYY_MM'));
    EXECUTE format(
        'CREATE TABLE IF NOT EXISTS %I PARTITION OF telemetry_env FOR VALUES FROM (%L) TO (%L)',
        part_name, start_ts, end_ts
    );
END;
$$;

CREATE OR REPLACE FUNCTION create_telemetry_soil_partition(p_year INT, p_month INT)
RETURNS VOID LANGUAGE plpgsql AS $$
DECLARE
    start_ts TIMESTAMPTZ;
    end_ts   TIMESTAMPTZ;
    part_name TEXT;
BEGIN
    start_ts := make_timestamptz(p_year, p_month, 1, 0, 0, 0, 'UTC');
    end_ts   := start_ts + INTERVAL '1 month';
    part_name := format('telemetry_soil_%s', to_char(start_ts, 'YYYY_MM'));
    EXECUTE format(
        'CREATE TABLE IF NOT EXISTS %I PARTITION OF telemetry_soil FOR VALUES FROM (%L) TO (%L)',
        part_name, start_ts, end_ts
    );
END;
$$;

DO $$
DECLARE
    d DATE := date_trunc('month', CURRENT_DATE AT TIME ZONE 'UTC')::date - INTERVAL '1 month';
    i INT;
BEGIN
    FOR i IN 0..4 LOOP
        PERFORM create_telemetry_env_partition(EXTRACT(YEAR FROM d)::INT, EXTRACT(MONTH FROM d)::INT);
        PERFORM create_telemetry_soil_partition(EXTRACT(YEAR FROM d)::INT, EXTRACT(MONTH FROM d)::INT);
        d := (d + INTERVAL '1 month')::date;
    END LOOP;
END $$;

CREATE INDEX IF NOT EXISTS idx_telemetry_env_received_brin ON telemetry_env USING BRIN (received_at);
CREATE INDEX IF NOT EXISTS idx_telemetry_soil_received_brin ON telemetry_soil USING BRIN (received_at);
