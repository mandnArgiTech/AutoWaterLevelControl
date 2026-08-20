package com.flm.platform.api.dto;

import java.time.Instant;
import java.util.UUID;

public record BindingSummary(
    UUID id,
    UUID assetId,
    UUID nodeCapabilityId,
    String role,
    Instant validFrom,
    Instant validTo
) {}
