package com.flm.platform.api.dto;

import java.time.Instant;
import java.util.UUID;

public record SiteSummary(
    UUID id,
    UUID vendorId,
    String name,
    String kind,
    Double latitude,
    Double longitude,
    String address,
    boolean active,
    Instant createdAt
) {}
