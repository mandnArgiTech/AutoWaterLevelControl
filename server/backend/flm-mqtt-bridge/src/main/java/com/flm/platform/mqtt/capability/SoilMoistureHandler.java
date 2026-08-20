package com.flm.platform.mqtt.capability;

import com.fasterxml.jackson.databind.JsonNode;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Component;

import java.sql.Timestamp;
import java.time.Instant;
import java.util.UUID;

@Component
public class SoilMoistureHandler implements CapabilityHandler {

    private static final Logger log = LoggerFactory.getLogger(SoilMoistureHandler.class);

    private final JdbcTemplate jdbc;

    public SoilMoistureHandler(JdbcTemplate jdbc) {
        this.jdbc = jdbc;
    }

    @Override
    public String capabilityKey() {
        return "measure.soil_moisture";
    }

    @Override
    public boolean handle(UUID deviceId, String deviceTag, Instant receivedAt, JsonNode root) {
        Float moisture = null;
        if (root.path("moisture_pct").isNumber()) moisture = (float) root.path("moisture_pct").asDouble();
        else if (root.path("moisturePct").isNumber()) moisture = (float) root.path("moisturePct").asDouble();
        else if (root.path("soil").path("moisture_pct").isNumber()) {
            moisture = (float) root.path("soil").path("moisture_pct").asDouble();
        }
        if (moisture != null && (moisture < 0 || moisture > 100)) {
            log.warn("Out-of-range moisture_pct={} for {}", moisture, deviceTag);
        }
        jdbc.update("""
            INSERT INTO telemetry_soil (device_id, received_at, moisture_pct)
            VALUES (?, ?, ?)
            """,
            deviceId, Timestamp.from(receivedAt), moisture
        );
        log.debug("SoilMoistureHandler wrote {} moisture={}", deviceTag, moisture);
        return true;
    }
}
