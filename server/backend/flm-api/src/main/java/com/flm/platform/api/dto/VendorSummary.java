package com.flm.platform.api.dto;

import java.time.Instant;
import java.util.UUID;

public record VendorSummary(UUID id, String code, String name, boolean active, Instant createdAt) {}
