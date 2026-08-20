package com.flm.platform.domain;

import jakarta.persistence.*;
import java.util.UUID;

@Entity
@Table(name = "metrics", uniqueConstraints = @UniqueConstraint(columnNames = {"capability_id", "metric_key"}))
public class Metric {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "capability_id", nullable = false)
    private Capability capability;

    @Column(name = "metric_key", nullable = false, length = 64)
    private String metricKey;

    @Column(nullable = false, length = 128)
    private String name;

    @Column(length = 32)
    private String unit;

    @Column(name = "data_type", nullable = false, length = 16)
    private String dataType = "float";

    @Column(name = "min_value")
    private Double minValue;

    @Column(name = "max_value")
    private Double maxValue;

    @Column(name = "display_hint", length = 64)
    private String displayHint;

    public UUID getId() { return id; }
    public Capability getCapability() { return capability; }
    public void setCapability(Capability capability) { this.capability = capability; }
    public String getMetricKey() { return metricKey; }
    public void setMetricKey(String metricKey) { this.metricKey = metricKey; }
    public String getName() { return name; }
    public void setName(String name) { this.name = name; }
    public String getUnit() { return unit; }
    public void setUnit(String unit) { this.unit = unit; }
    public String getDataType() { return dataType; }
    public void setDataType(String dataType) { this.dataType = dataType; }
    public Double getMinValue() { return minValue; }
    public void setMinValue(Double minValue) { this.minValue = minValue; }
    public Double getMaxValue() { return maxValue; }
    public void setMaxValue(Double maxValue) { this.maxValue = maxValue; }
    public String getDisplayHint() { return displayHint; }
    public void setDisplayHint(String displayHint) { this.displayHint = displayHint; }
}
