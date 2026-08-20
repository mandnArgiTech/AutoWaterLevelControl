package com.flm.platform.api.dto;

import java.time.Instant;
import java.util.UUID;

public record DeviceModelSummary(
    UUID id,
    String modelKey,
    String name,
    String role,
    String board,
    String mcu,
    String defaultComm,
    String capabilityProfile,
    String protectionProfile,
    String interlockTier,
    Instant createdAt
) {}
