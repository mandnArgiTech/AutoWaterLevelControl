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
    Long uptimeMs,
    String lifecycleState,
    String modelKey,
    UUID siteId,
    Integer freeHeap,
    Integer minFreeHeap,
    Integer maxFreeBlock,
    Short rssi,
    String ipAddress,
    String firmware,
    Instant healthReceivedAt
) {
    /** Compat constructor used when health/model fields are unknown. */
    public DeviceSummary(
        UUID id,
        String deviceTag,
        String displayName,
        boolean online,
        Instant lastSeenAt,
        Double latestPercentFilled,
        Double latestVolumeLiters,
        Instant latestReadingAt,
        String uptime,
        Long uptimeMs
    ) {
        this(id, deviceTag, displayName, online, lastSeenAt, latestPercentFilled, latestVolumeLiters,
            latestReadingAt, uptime, uptimeMs, null, null, null, null, null, null, null, null, null, null);
    }
}
