CREATE TABLE IF NOT EXISTS platform_audit_log (
    id UUID PRIMARY KEY,
    actor_user_id UUID REFERENCES users(id),
    action VARCHAR(64) NOT NULL,
    resource VARCHAR(128) NOT NULL,
    detail JSONB,
    ip_address VARCHAR(64),
    user_agent VARCHAR(512),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_audit_created ON platform_audit_log(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_audit_actor ON platform_audit_log(actor_user_id);

-- Revoke update/delete on audit log from app role (immutable)
REVOKE UPDATE, DELETE ON platform_audit_log FROM PUBLIC;
