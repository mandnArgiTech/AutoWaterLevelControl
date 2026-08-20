package com.flm.platform.api.service;

import com.flm.platform.api.dto.CapabilitySummary;
import com.flm.platform.api.dto.MetricSummary;
import com.flm.platform.domain.Capability;
import com.flm.platform.domain.CapabilityRepository;
import com.flm.platform.domain.MetricRepository;
import com.flm.platform.security.TenantGuard;
import org.springframework.stereotype.Service;
import java.util.List;

@Service
public class CapabilityService {

    private final CapabilityRepository capabilityRepository;
    private final MetricRepository metricRepository;
    private final TenantGuard tenantGuard;

    public CapabilityService(
        CapabilityRepository capabilityRepository,
        MetricRepository metricRepository,
        TenantGuard tenantGuard
    ) {
        this.capabilityRepository = capabilityRepository;
        this.metricRepository = metricRepository;
        this.tenantGuard = tenantGuard;
    }

    public List<CapabilitySummary> list() {
        tenantGuard.current();
        return capabilityRepository.findAll().stream()
            .sorted((a, b) -> a.getCapKey().compareTo(b.getCapKey()))
            .map(this::toSummary)
            .toList();
    }

    private CapabilitySummary toSummary(Capability c) {
        List<MetricSummary> metrics = metricRepository.findByCapabilityIdOrderByMetricKeyAsc(c.getId()).stream()
            .map(m -> new MetricSummary(
                m.getMetricKey(), m.getName(), m.getUnit(), m.getDataType(),
                m.getMinValue(), m.getMaxValue(), m.getDisplayHint()
            ))
            .toList();
        return new CapabilitySummary(c.getId(), c.getCapKey(), c.getName(), c.getCategory(), c.getDescription(), metrics);
    }
}
