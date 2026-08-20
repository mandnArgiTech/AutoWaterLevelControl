package com.flm.platform.mqtt.capability;

import com.fasterxml.jackson.databind.JsonNode;
import java.time.Instant;
import java.util.UUID;

/**
 * Pluggable ingest handler for one capability key (e.g. measure.water_level).
 */
public interface CapabilityHandler {

    /** Capability registry key this handler owns. */
    String capabilityKey();

    /**
     * Persist / enqueue a telemetry payload for the device.
     * @return true if handled
     */
    boolean handle(UUID deviceId, String deviceTag, Instant receivedAt, JsonNode root);
}
