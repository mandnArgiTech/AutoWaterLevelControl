package com.flm.platform.common;

public record PageResponse<T>(java.util.List<T> items, long total, int page, int size) {}
