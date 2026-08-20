-- V10: Logical topology — assets, flow edges, bindings (FLM-L.1).
-- Control policies deferred to M2; coordination_group table only.

CREATE TABLE IF NOT EXISTS asset (
    id              UUID PRIMARY KEY,
    site_id         UUID NOT NULL REFERENCES sites(id) ON DELETE CASCADE,
    name            VARCHAR(255) NOT NULL,
    kind            VARCHAR(32) NOT NULL,   -- TANK | PUMP | VALVE | SOURCE | GENERIC
    subtype         VARCHAR(64),            -- SUMP | OVERHEAD | STORAGE | MUNICIPAL | ...
    capacity_l      DOUBLE PRECISION,
    geometry_json   JSONB,
    attributes      JSONB,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_asset_site ON asset(site_id);

CREATE TABLE IF NOT EXISTS flow_edge (
    id              UUID PRIMARY KEY,
    site_id         UUID NOT NULL REFERENCES sites(id) ON DELETE CASCADE,
    from_asset_id   UUID NOT NULL REFERENCES asset(id) ON DELETE CASCADE,
    to_asset_id     UUID NOT NULL REFERENCES asset(id) ON DELETE CASCADE,
    via_asset_id    UUID REFERENCES asset(id) ON DELETE SET NULL,
    kind            VARCHAR(32) NOT NULL DEFAULT 'FLOW',  -- FLOW | CONTROL
    attributes      JSONB,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_flow_edge_site ON flow_edge(site_id);

CREATE TABLE IF NOT EXISTS coordination_group (
    id          UUID PRIMARY KEY,
    site_id     UUID NOT NULL REFERENCES sites(id) ON DELETE CASCADE,
    name        VARCHAR(128) NOT NULL,
    strategy    VARCHAR(32) NOT NULL DEFAULT 'NONE',  -- unused until M2
    attributes  JSONB,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS binding (
    id                  UUID PRIMARY KEY,
    asset_id            UUID NOT NULL REFERENCES asset(id) ON DELETE CASCADE,
    node_capability_id  UUID NOT NULL REFERENCES node_capabilities(id) ON DELETE CASCADE,
    role                VARCHAR(16) NOT NULL
        CHECK (role IN ('MEASURES', 'ACTUATES')),
    valid_from          TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    valid_to            TIMESTAMPTZ,
    created_at          TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_binding_asset ON binding(asset_id);
CREATE INDEX IF NOT EXISTS idx_binding_channel ON binding(node_capability_id);

-- Prevent overlapping active bindings for the same channel+role
CREATE UNIQUE INDEX IF NOT EXISTS uq_binding_active_channel_role
    ON binding (node_capability_id, role)
    WHERE valid_to IS NULL;
