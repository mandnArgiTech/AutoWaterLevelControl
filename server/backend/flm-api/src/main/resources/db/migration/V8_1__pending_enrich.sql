-- V8.1: Enrich pending_devices for birth/announce (FLM-F.6).
ALTER TABLE pending_devices
    ADD COLUMN IF NOT EXISTS model_key VARCHAR(64);

CREATE INDEX IF NOT EXISTS idx_pending_devices_model_key
    ON pending_devices (model_key)
    WHERE model_key IS NOT NULL;
