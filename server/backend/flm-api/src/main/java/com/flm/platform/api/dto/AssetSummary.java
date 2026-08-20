package com.flm.platform.api.dto;

import java.util.UUID;

public record AssetSummary(
    UUID id,
    UUID siteId,
    String name,
    String kind,
    String subtype,
    Double capacityL,
    String attributes
) {}
