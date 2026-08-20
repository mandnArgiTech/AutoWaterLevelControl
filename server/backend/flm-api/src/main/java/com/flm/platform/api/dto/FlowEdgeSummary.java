package com.flm.platform.api.dto;

import java.util.UUID;

public record FlowEdgeSummary(
    UUID id,
    UUID siteId,
    UUID fromAssetId,
    UUID toAssetId,
    UUID viaAssetId,
    String kind
) {}
