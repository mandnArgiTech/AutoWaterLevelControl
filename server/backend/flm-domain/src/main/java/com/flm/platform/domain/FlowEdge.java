package com.flm.platform.domain;

import jakarta.persistence.*;
import org.hibernate.annotations.JdbcTypeCode;
import org.hibernate.type.SqlTypes;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "flow_edge")
public class FlowEdge {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "site_id", nullable = false)
    private Site site;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "from_asset_id", nullable = false)
    private Asset fromAsset;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "to_asset_id", nullable = false)
    private Asset toAsset;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "via_asset_id")
    private Asset viaAsset;

    @Column(nullable = false, length = 32)
    private String kind = "FLOW";

    @JdbcTypeCode(SqlTypes.JSON)
    @Column(columnDefinition = "jsonb")
    private String attributes;

    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt = Instant.now();

    public UUID getId() { return id; }
    public Site getSite() { return site; }
    public void setSite(Site site) { this.site = site; }
    public Asset getFromAsset() { return fromAsset; }
    public void setFromAsset(Asset fromAsset) { this.fromAsset = fromAsset; }
    public Asset getToAsset() { return toAsset; }
    public void setToAsset(Asset toAsset) { this.toAsset = toAsset; }
    public Asset getViaAsset() { return viaAsset; }
    public void setViaAsset(Asset viaAsset) { this.viaAsset = viaAsset; }
    public String getKind() { return kind; }
    public void setKind(String kind) { this.kind = kind; }
    public String getAttributes() { return attributes; }
    public void setAttributes(String attributes) { this.attributes = attributes; }
    public Instant getCreatedAt() { return createdAt; }
}
