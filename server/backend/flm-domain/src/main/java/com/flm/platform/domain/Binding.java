package com.flm.platform.domain;

import jakarta.persistence.*;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "binding")
public class Binding {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "asset_id", nullable = false)
    private Asset asset;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "node_capability_id", nullable = false)
    private NodeCapability nodeCapability;

    @Column(nullable = false, length = 16)
    private String role;

    @Column(name = "valid_from", nullable = false)
    private Instant validFrom = Instant.now();

    @Column(name = "valid_to")
    private Instant validTo;

    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt = Instant.now();

    public UUID getId() { return id; }
    public Asset getAsset() { return asset; }
    public void setAsset(Asset asset) { this.asset = asset; }
    public NodeCapability getNodeCapability() { return nodeCapability; }
    public void setNodeCapability(NodeCapability nodeCapability) { this.nodeCapability = nodeCapability; }
    public String getRole() { return role; }
    public void setRole(String role) { this.role = role; }
    public Instant getValidFrom() { return validFrom; }
    public void setValidFrom(Instant validFrom) { this.validFrom = validFrom; }
    public Instant getValidTo() { return validTo; }
    public void setValidTo(Instant validTo) { this.validTo = validTo; }
}
