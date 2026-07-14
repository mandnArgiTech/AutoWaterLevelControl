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
    Instant latestReadingAt,
    /** Human-readable device uptime, e.g. "2d 5h 30m". */
    String uptime,
    Long uptimeMs
) {}
