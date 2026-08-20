package com.flm.platform.domain;

import org.springframework.data.jpa.repository.JpaRepository;
import java.util.List;
import java.util.UUID;

public interface MetricRepository extends JpaRepository<Metric, UUID> {
    List<Metric> findByCapabilityIdOrderByMetricKeyAsc(UUID capabilityId);
}
