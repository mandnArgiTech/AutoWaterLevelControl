package com.flm.platform.api.dto;

import java.time.Instant;
import java.util.UUID;

public record DeviceSummary(
    UUID id,
    String deviceTag,
    String displayName,
    boolean online,
    Instant lastSeenAt,
    Double latestPercentFilled,
    Double latestVolumeLiters,
    Instant latestReadingAt
) {}
