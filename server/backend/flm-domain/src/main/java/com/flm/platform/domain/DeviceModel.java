package com.flm.platform.domain;

import jakarta.persistence.*;
import org.hibernate.annotations.JdbcTypeCode;
import org.hibernate.type.SqlTypes;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "device_model", uniqueConstraints = @UniqueConstraint(columnNames = "model_key"))
public class DeviceModel {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(name = "model_key", nullable = false, length = 64)
    private String modelKey;

    @Column(nullable = false, length = 128)
    private String name;

    @Column(nullable = false, length = 16)
    private String role;

    @Column(length = 64)
    private String board;

    @Column(length = 32)
    private String mcu;

    @Column(name = "default_comm", nullable = false, length = 16)
    private String defaultComm = "wifi";

    @JdbcTypeCode(SqlTypes.JSON)
    @Column(name = "capability_profile", columnDefinition = "jsonb")
    private String capabilityProfile;

    @JdbcTypeCode(SqlTypes.JSON)
    @Column(name = "protection_profile", columnDefinition = "jsonb")
    private String protectionProfile;

    @Column(name = "interlock_tier", nullable = false, length = 16)
    private String interlockTier = "none";

    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt = Instant.now();

    public UUID getId() { return id; }
    public String getModelKey() { return modelKey; }
    public void setModelKey(String modelKey) { this.modelKey = modelKey; }
    public String getName() { return name; }
    public void setName(String name) { this.name = name; }
    public String getRole() { return role; }
    public void setRole(String role) { this.role = role; }
    public String getBoard() { return board; }
    public void setBoard(String board) { this.board = board; }
    public String getMcu() { return mcu; }
    public void setMcu(String mcu) { this.mcu = mcu; }
    public String getDefaultComm() { return defaultComm; }
    public void setDefaultComm(String defaultComm) { this.defaultComm = defaultComm; }
    public String getCapabilityProfile() { return capabilityProfile; }
    public void setCapabilityProfile(String capabilityProfile) { this.capabilityProfile = capabilityProfile; }
    public String getProtectionProfile() { return protectionProfile; }
    public void setProtectionProfile(String protectionProfile) { this.protectionProfile = protectionProfile; }
    public String getInterlockTier() { return interlockTier; }
    public void setInterlockTier(String interlockTier) { this.interlockTier = interlockTier; }
    public Instant getCreatedAt() { return createdAt; }
}
