package com.flm.platform.api.dto;

import java.time.Instant;

public record ReadingPoint(
    Instant receivedAt,
    Double percentFilled,
    Double volumeLiters,
    Double temperatureC
) {}
