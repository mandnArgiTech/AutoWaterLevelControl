package com.flm.platform.api.dto;

import java.time.Instant;
import java.util.UUID;

public record ReadingPoint(
    UUID id,
    Instant receivedAt,
    Double percentFilled,
    Double volumeLiters,
    Double temperatureC,
    String payloadJson
) {}
