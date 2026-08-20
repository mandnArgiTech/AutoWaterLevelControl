package com.flm.platform.domain;

import com.flm.platform.common.CommType;
import com.flm.platform.common.LifecycleState;
import jakarta.persistence.*;
import org.hibernate.annotations.JdbcTypeCode;
import org.hibernate.type.SqlTypes;
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

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "model_id")
    private DeviceModel model;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "site_id")
    private Site site;

    @Enumerated(EnumType.STRING)
    @Column(name = "comm_type", nullable = false, length = 16)
    private CommType commType = CommType.wifi;

    @Enumerated(EnumType.STRING)
    @Column(name = "lifecycle_state", nullable = false, length = 16)
    private LifecycleState lifecycleState = LifecycleState.ACTIVE;

    @JdbcTypeCode(SqlTypes.JSON)
    @Column(columnDefinition = "jsonb")
    private String attributes;

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
    public DeviceModel getModel() { return model; }
    public void setModel(DeviceModel model) { this.model = model; }
    public Site getSite() { return site; }
    public void setSite(Site site) { this.site = site; }
    public CommType getCommType() { return commType; }
    public void setCommType(CommType commType) { this.commType = commType; }
    public LifecycleState getLifecycleState() { return lifecycleState; }
    public void setLifecycleState(LifecycleState lifecycleState) { this.lifecycleState = lifecycleState; }
    public String getAttributes() { return attributes; }
    public void setAttributes(String attributes) { this.attributes = attributes; }
    public boolean isOnline() { return online; }
    public void setOnline(boolean online) { this.online = online; }
    public Instant getLastSeenAt() { return lastSeenAt; }
    public void setLastSeenAt(Instant lastSeenAt) { this.lastSeenAt = lastSeenAt; }
    public Long getUptimeMs() { return uptimeMs; }
    public void setUptimeMs(Long uptimeMs) { this.uptimeMs = uptimeMs; }
    public Instant getRegisteredAt() { return registeredAt; }
}
