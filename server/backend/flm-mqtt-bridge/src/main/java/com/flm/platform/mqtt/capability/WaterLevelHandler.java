package com.flm.platform.mqtt.capability;

import com.fasterxml.jackson.databind.JsonNode;
import com.flm.platform.mqtt.telemetry.ReadingBatchWriter;
import com.flm.platform.mqtt.telemetry.TelemetrySample;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

import java.time.Instant;
import java.util.UUID;

/** Writes water level into existing level_readings / device_latest. */
@Component
public class WaterLevelHandler implements CapabilityHandler {

    private static final Logger log = LoggerFactory.getLogger(WaterLevelHandler.class);

    private final ReadingBatchWriter batchWriter;

    public WaterLevelHandler(ReadingBatchWriter batchWriter) {
        this.batchWriter = batchWriter;
    }

    @Override
    public String capabilityKey() {
        return "measure.water_level";
    }

    @Override
    public boolean handle(UUID deviceId, String deviceTag, Instant receivedAt, JsonNode root) {
        JsonNode level = root.has("level") ? root.path("level") : root;
        JsonNode sensor = root.path("sensor");
        JsonNode battery = root.path("battery");

        Float pct = numFloat(level, "percentFilled", "percent_filled");
        if (pct != null && (pct < 0 || pct > 100)) {
            log.warn("Out-of-range percent_filled={} for {}", pct, deviceTag);
        }
        Float vol = numFloat(level, "volumeLiters", "volume_liters");
        Short waterMm = toShort(level, "waterHeightMm", "water_height_mm");
        if (waterMm == null && level.path("waterHeightCm").isNumber()) {
            waterMm = (short) Math.round(level.path("waterHeightCm").asDouble() * 10.0);
        }
        Short distMm = toShort(sensor, "distanceMm", "distance_mm");
        if (distMm == null && sensor.path("distanceCm").isNumber()) {
            distMm = (short) Math.round(sensor.path("distanceCm").asDouble() * 10.0);
        }
        Float tempC = numFloat(sensor, "temperatureC", "temperature_c");
        Short humidity = toShort(sensor, "humidityPct", "humidity_pct");
        Short battPct = toShort(battery, "percent");
        Short battMv = null;
        if (battery.path("voltage").isNumber()) {
            battMv = (short) Math.round(battery.path("voltage").asDouble() * 1000.0);
        } else if (battery.path("millivolts").isNumber()) {
            battMv = (short) battery.path("millivolts").asInt();
        }
        Long uptimeMs = root.path("uptimeMs").isNumber() ? root.path("uptimeMs").asLong()
            : (root.path("uptime").isNumber() ? root.path("uptime").asLong() : null);
        Short rssi = root.path("rssi").isNumber() ? (short) root.path("rssi").asInt() : null;

        batchWriter.enqueue(new TelemetrySample(
            deviceId, receivedAt,
            pct, vol, waterMm, distMm, tempC, humidity, battMv, battPct,
            uptimeMs, rssi, true, true
        ));
        log.debug("WaterLevelHandler queued {} pct={}", deviceTag, pct);
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
