package com.flm.platform.domain;

import jakarta.persistence.*;
import org.hibernate.annotations.JdbcTypeCode;
import org.hibernate.type.SqlTypes;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "asset")
public class Asset {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "site_id", nullable = false)
    private Site site;

    @Column(nullable = false)
    private String name;

    @Column(nullable = false, length = 32)
    private String kind;

    @Column(length = 64)
    private String subtype;

    @Column(name = "capacity_l")
    private Double capacityL;

    @JdbcTypeCode(SqlTypes.JSON)
    @Column(name = "geometry_json", columnDefinition = "jsonb")
    private String geometryJson;

    @JdbcTypeCode(SqlTypes.JSON)
    @Column(columnDefinition = "jsonb")
    private String attributes;

    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt = Instant.now();

    public UUID getId() { return id; }
    public Site getSite() { return site; }
    public void setSite(Site site) { this.site = site; }
    public String getName() { return name; }
    public void setName(String name) { this.name = name; }
    public String getKind() { return kind; }
    public void setKind(String kind) { this.kind = kind; }
    public String getSubtype() { return subtype; }
    public void setSubtype(String subtype) { this.subtype = subtype; }
    public Double getCapacityL() { return capacityL; }
    public void setCapacityL(Double capacityL) { this.capacityL = capacityL; }
    public String getGeometryJson() { return geometryJson; }
    public void setGeometryJson(String geometryJson) { this.geometryJson = geometryJson; }
    public String getAttributes() { return attributes; }
    public void setAttributes(String attributes) { this.attributes = attributes; }
    public Instant getCreatedAt() { return createdAt; }
}
