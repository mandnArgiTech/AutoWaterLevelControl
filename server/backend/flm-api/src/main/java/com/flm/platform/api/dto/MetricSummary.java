package com.flm.platform.api.dto;

public record MetricSummary(
    String key,
    String name,
    String unit,
    String dataType,
    Double minValue,
    Double maxValue,
    String displayHint
) {}
