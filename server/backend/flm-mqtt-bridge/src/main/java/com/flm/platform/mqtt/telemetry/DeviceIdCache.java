package com.flm.platform.mqtt.telemetry;

import com.flm.platform.domain.Device;
import com.flm.platform.domain.DeviceRepository;
import org.springframework.stereotype.Component;
import java.util.Optional;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Cache deviceTag → id to avoid a DB round-trip per MQTT message.
 */
@Component
public class DeviceIdCache {

    private static final long TTL_MS = 10 * 60 * 1000L;

    private record Entry(UUID deviceId, String topicPrefix, long expiresAtMs) {}

    private final DeviceRepository deviceRepository;
    private final ConcurrentHashMap<String, Entry> byTag = new ConcurrentHashMap<>();

    public DeviceIdCache(DeviceRepository deviceRepository) {
        this.deviceRepository = deviceRepository;
    }

    public Optional<CachedDevice> lookup(String deviceTag) {
        if (deviceTag == null || deviceTag.isBlank()) return Optional.empty();
        long now = System.currentTimeMillis();
        Entry cached = byTag.get(deviceTag);
        if (cached != null && cached.expiresAtMs > now) {
            return Optional.of(new CachedDevice(cached.deviceId, cached.topicPrefix));
        }
        Optional<Device> db = deviceRepository.findByDeviceTag(deviceTag);
        if (db.isEmpty()) {
            byTag.remove(deviceTag);
            return Optional.empty();
        }
        Device d = db.get();
        String prefix = d.getTopicPrefix() != null ? d.getTopicPrefix() : "water";
        byTag.put(deviceTag, new Entry(d.getId(), prefix, now + TTL_MS));
        return Optional.of(new CachedDevice(d.getId(), prefix));
    }

    public void invalidate(String deviceTag) {
        if (deviceTag != null) byTag.remove(deviceTag);
    }

    public record CachedDevice(UUID deviceId, String topicPrefix) {}
}
