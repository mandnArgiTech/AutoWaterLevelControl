package com.flm.platform.domain;

import jakarta.persistence.*;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "devices", uniqueConstraints = @UniqueConstraint(columnNames = "device_tag"))
public class Device {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "vendor_id", nullable = false)
    private Vendor vendor;

    /** Matches ESP8266 MQTT tag: {deviceName}_{chipId} */
    @Column(name = "device_tag", nullable = false, length = 128)
    private String deviceTag;

    @Column(nullable = false)
    private String displayName;

    private String chipId;
    private String topicPrefix = "water";

    @Column(nullable = false)
    private boolean online;

    private Instant lastSeenAt;

    /** Last reported ESP8266 uptime from MQTT status/level (milliseconds). */
    private Long uptimeMs;

    @Column(nullable = false, updatable = false)
    private Instant registeredAt = Instant.now();

    public UUID getId() { return id; }
    public Vendor getVendor() { return vendor; }
    public void setVendor(Vendor vendor) { this.vendor = vendor; }
    public String getDeviceTag() { return deviceTag; }
    public void setDeviceTag(String deviceTag) { this.deviceTag = deviceTag; }
    public String getDisplayName() { return displayName; }
    public void setDisplayName(String displayName) { this.displayName = displayName; }
    public String getChipId() { return chipId; }
    public void setChipId(String chipId) { this.chipId = chipId; }
    public String getTopicPrefix() { return topicPrefix; }
    public void setTopicPrefix(String topicPrefix) { this.topicPrefix = topicPrefix; }
    public boolean isOnline() { return online; }
    public void setOnline(boolean online) { this.online = online; }
    public Instant getLastSeenAt() { return lastSeenAt; }
    public void setLastSeenAt(Instant lastSeenAt) { this.lastSeenAt = lastSeenAt; }
    public Long getUptimeMs() { return uptimeMs; }
    public void setUptimeMs(Long uptimeMs) { this.uptimeMs = uptimeMs; }
    public Instant getRegisteredAt() { return registeredAt; }
}
