package com.flm.platform.domain;

import jakarta.persistence.*;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "lifecycle_event")
public class LifecycleEvent {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(name = "device_id", nullable = false)
    private UUID deviceId;

    @Column(name = "from_state", length = 16)
    private String fromState;

    @Column(name = "to_state", nullable = false, length = 16)
    private String toState;

    @Column(length = 255)
    private String reason;

    @Column(length = 128)
    private String actor;

    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt = Instant.now();

    public UUID getId() { return id; }
    public UUID getDeviceId() { return deviceId; }
    public void setDeviceId(UUID deviceId) { this.deviceId = deviceId; }
    public String getFromState() { return fromState; }
    public void setFromState(String fromState) { this.fromState = fromState; }
    public String getToState() { return toState; }
    public void setToState(String toState) { this.toState = toState; }
    public String getReason() { return reason; }
    public void setReason(String reason) { this.reason = reason; }
    public String getActor() { return actor; }
    public void setActor(String actor) { this.actor = actor; }
    public Instant getCreatedAt() { return createdAt; }
}
