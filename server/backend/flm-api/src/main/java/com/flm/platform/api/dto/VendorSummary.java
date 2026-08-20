package com.flm.platform.api.dto;

import com.flm.platform.common.VendorType;
import java.time.Instant;
import java.util.UUID;

public record VendorSummary(
    UUID id,
    String code,
    String name,
    VendorType vendorType,
    boolean active,
    Instant createdAt
) {}
