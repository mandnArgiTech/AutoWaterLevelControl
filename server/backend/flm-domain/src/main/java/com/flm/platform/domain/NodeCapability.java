package com.flm.platform.domain;

import jakarta.persistence.*;
import org.hibernate.annotations.JdbcTypeCode;
import org.hibernate.type.SqlTypes;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "node_capabilities", uniqueConstraints = @UniqueConstraint(
    columnNames = {"device_id", "capability_id", "chan_index"}))
public class NodeCapability {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "device_id", nullable = false)
    private Device device;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "capability_id", nullable = false)
    private Capability capability;

    @Column(name = "chan_index", nullable = false)
    private short chanIndex;

    @Column(length = 128)
    private String name;

    @JdbcTypeCode(SqlTypes.JSON)
    @Column(columnDefinition = "jsonb")
    private String attributes;

    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt = Instant.now();

    public UUID getId() { return id; }
    public Device getDevice() { return device; }
    public void setDevice(Device device) { this.device = device; }
    public Capability getCapability() { return capability; }
    public void setCapability(Capability capability) { this.capability = capability; }
    public short getChanIndex() { return chanIndex; }
    public void setChanIndex(short chanIndex) { this.chanIndex = chanIndex; }
    public String getName() { return name; }
    public void setName(String name) { this.name = name; }
    public String getAttributes() { return attributes; }
    public void setAttributes(String attributes) { this.attributes = attributes; }
}
