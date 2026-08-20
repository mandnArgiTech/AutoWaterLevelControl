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
public class EnvHandler implements CapabilityHandler {

    private static final Logger log = LoggerFactory.getLogger(EnvHandler.class);

    private final JdbcTemplate jdbc;

    public EnvHandler(JdbcTemplate jdbc) {
        this.jdbc = jdbc;
    }

    @Override
    public String capabilityKey() {
        return "measure.env";
    }

    @Override
    public boolean handle(UUID deviceId, String deviceTag, Instant receivedAt, JsonNode root) {
        JsonNode sensor = root.has("sensor") ? root.path("sensor") : root;
        Float tempC = numFloat(sensor, "temperatureC", "temperature_c", "temp_c");
        Short humidity = toShort(sensor, "humidityPct", "humidity_pct", "humidity");
        if (tempC != null && (tempC < -40 || tempC > 85)) {
            log.warn("Out-of-range temperature_c={} for {}", tempC, deviceTag);
        }
        if (humidity != null && (humidity < 0 || humidity > 100)) {
            log.warn("Out-of-range humidity_pct={} for {}", humidity, deviceTag);
        }
        jdbc.update("""
            INSERT INTO telemetry_env (device_id, received_at, temperature_c, humidity_pct)
            VALUES (?, ?, ?, ?)
            """,
            deviceId, Timestamp.from(receivedAt), tempC, humidity
        );
        log.debug("EnvHandler wrote {} temp={} humidity={}", deviceTag, tempC, humidity);
        return true;
    }

    private static Float numFloat(JsonNode n, String... fields) {
        for (String f : fields) {
            if (n.path(f).isNumber()) return (float) n.path(f).asDouble();
        }
        return null;
    }

    private static Short toShort(JsonNode n, String... fields) {
        for (String f : fields) {
            if (n.path(f).isNumber()) {
                int v = n.path(f).asInt();
                if (v >= Short.MIN_VALUE && v <= Short.MAX_VALUE) return (short) v;
            }
        }
        return null;
    }
}
