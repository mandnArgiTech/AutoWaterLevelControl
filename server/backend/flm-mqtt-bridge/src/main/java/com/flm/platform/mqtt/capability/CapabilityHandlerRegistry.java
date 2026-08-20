package com.flm.platform.mqtt.capability;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;

@Component
public class CapabilityHandlerRegistry {

    private static final Logger log = LoggerFactory.getLogger(CapabilityHandlerRegistry.class);

    private final Map<String, CapabilityHandler> byKey = new HashMap<>();

    public CapabilityHandlerRegistry(List<CapabilityHandler> handlers) {
        for (CapabilityHandler h : handlers) {
            byKey.put(h.capabilityKey(), h);
            log.info("Registered capability handler: {}", h.capabilityKey());
        }
    }

    public Optional<CapabilityHandler> find(String capabilityKey) {
        if (capabilityKey == null || capabilityKey.isBlank()) return Optional.empty();
        return Optional.ofNullable(byKey.get(capabilityKey));
    }
}
