-- V8: Capture MQTT messages from unknown device tags (Phase 0 / FLM-S.6).
-- Previously dropped with a warn log; now upserted so operators can approve/register.

CREATE TABLE IF NOT EXISTS pending_devices (
    device_tag      VARCHAR(128) PRIMARY KEY,
    chip_id         VARCHAR(64),
    topic_prefix    VARCHAR(64),
    sample_topic    VARCHAR(255),
    sample_payload  TEXT,
    sample_count    BIGINT NOT NULL DEFAULT 1,
    first_seen_at   TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_seen_at    TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_pending_devices_last_seen
    ON pending_devices (last_seen_at DESC);
