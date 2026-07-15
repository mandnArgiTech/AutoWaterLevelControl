-- V6: Typed, partitioned telemetry for 18k-tank / 5-year scale.
-- Replaces JSON blob storage. Truncates existing reading history (fresh start).

DROP TABLE IF EXISTS level_readings CASCADE;

CREATE TABLE level_readings (
    device_id           UUID NOT NULL,
    received_at         TIMESTAMPTZ NOT NULL,
    percent_filled      REAL,
    volume_liters       REAL,
    water_height_mm     SMALLINT,
    distance_mm         SMALLINT,
    temperature_c       REAL,
    humidity_pct        SMALLINT,
    battery_mv          SMALLINT,
    battery_percent     SMALLINT,
    PRIMARY KEY (device_id, received_at)
) PARTITION BY RANGE (received_at);

CREATE TABLE IF NOT EXISTS device_latest (
    device_id           UUID PRIMARY KEY,
    received_at         TIMESTAMPTZ NOT NULL,
    percent_filled      REAL,
    volume_liters       REAL,
    water_height_mm     SMALLINT,
    distance_mm         SMALLINT,
    temperature_c       REAL,
    humidity_pct        SMALLINT,
    battery_mv          SMALLINT,
    battery_percent     SMALLINT,
    uptime_ms           BIGINT,
    rssi                SMALLINT,
    online              BOOLEAN NOT NULL DEFAULT FALSE
);

-- Helper: create a monthly partition if missing (used by app partition job too)
CREATE OR REPLACE FUNCTION create_level_readings_partition(p_year INT, p_month INT)
RETURNS VOID
LANGUAGE plpgsql
AS $$
DECLARE
    start_ts TIMESTAMPTZ;
    end_ts   TIMESTAMPTZ;
    part_name TEXT;
BEGIN
    start_ts := make_timestamptz(p_year, p_month, 1, 0, 0, 0, 'UTC');
    end_ts   := start_ts + INTERVAL '1 month';
    part_name := format('level_readings_%s', to_char(start_ts, 'YYYY_MM'));
    EXECUTE format(
        'CREATE TABLE IF NOT EXISTS %I PARTITION OF level_readings FOR VALUES FROM (%L) TO (%L)',
        part_name, start_ts, end_ts
    );
END;
$$;

-- Pre-create previous month, current, and next 3 months
DO $$
DECLARE
    d DATE := date_trunc('month', CURRENT_DATE AT TIME ZONE 'UTC')::date - INTERVAL '1 month';
    i INT;
BEGIN
    FOR i IN 0..4 LOOP
        PERFORM create_level_readings_partition(
            EXTRACT(YEAR FROM d)::INT,
            EXTRACT(MONTH FROM d)::INT
        );
        d := (d + INTERVAL '1 month')::date;
    END LOOP;
END $$;

-- BRIN on parent covers partitions for cross-device range scans / AI export
CREATE INDEX IF NOT EXISTS idx_level_readings_received_at_brin
    ON level_readings USING BRIN (received_at);

CREATE INDEX IF NOT EXISTS idx_device_latest_received_at
    ON device_latest (received_at DESC);
