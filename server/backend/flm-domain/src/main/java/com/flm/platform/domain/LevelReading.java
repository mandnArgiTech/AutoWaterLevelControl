package com.flm.platform.domain;

import jakarta.persistence.*;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "level_readings", indexes = {
    @Index(name = "idx_readings_device_time", columnList = "device_id, received_at DESC")
})
public class LevelReading {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "device_id", nullable = false)
    private Device device;

    @Column(nullable = false, columnDefinition = "TEXT")
    private String payloadJson;

    private Double percentFilled;
    private Double volumeLiters;
    private Double temperatureC;

    @Column(nullable = false)
    private Instant receivedAt = Instant.now();

    public UUID getId() { return id; }
    public Device getDevice() { return device; }
    public void setDevice(Device device) { this.device = device; }
    public String getPayloadJson() { return payloadJson; }
    public void setPayloadJson(String payloadJson) { this.payloadJson = payloadJson; }
    public Double getPercentFilled() { return percentFilled; }
    public void setPercentFilled(Double percentFilled) { this.percentFilled = percentFilled; }
    public Double getVolumeLiters() { return volumeLiters; }
    public void setVolumeLiters(Double volumeLiters) { this.volumeLiters = volumeLiters; }
    public Double getTemperatureC() { return temperatureC; }
    public void setTemperatureC(Double temperatureC) { this.temperatureC = temperatureC; }
    public Instant getReceivedAt() { return receivedAt; }
}
