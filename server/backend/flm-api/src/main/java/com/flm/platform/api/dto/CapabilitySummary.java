package com.flm.platform.api.dto;

import java.util.List;
import java.util.UUID;

public record CapabilitySummary(
    UUID id,
    String key,
    String name,
    String category,
    String description,
    List<MetricSummary> metrics
) {}
