package com.flm.platform.mqtt.telemetry;

import java.time.Instant;
import java.util.UUID;

/**
 * Narrow typed sample ready for batch JDBC insert (no JSON).
 */
public record TelemetrySample(
    UUID deviceId,
    Instant receivedAt,
    Float percentFilled,
    Float volumeLiters,
    Short waterHeightMm,
    Short distanceMm,
    Float temperatureC,
    Short humidityPct,
    Short batteryMv,
    Short batteryPercent,
    Long uptimeMs,
    Short rssi,
    boolean online,
    boolean isLevelReading
) {}
