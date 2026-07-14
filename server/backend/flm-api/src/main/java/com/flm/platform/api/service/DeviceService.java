package com.flm.platform.api.service;

import com.flm.platform.api.dto.*;
import com.flm.platform.common.PageResponse;
import com.flm.platform.common.UserRole;
import com.flm.platform.domain.*;
import com.flm.platform.mqtt.MqttIngestService;
import com.flm.platform.security.AuthPrincipal;
import com.flm.platform.security.TenantGuard;
import org.springframework.data.domain.PageRequest;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import java.time.Instant;
import java.util.List;
import java.util.UUID;

@Service
public class DeviceService {

    private final DeviceRepository deviceRepository;
    private final LevelReadingRepository readingRepository;
    private final VendorRepository vendorRepository;
    private final TenantGuard tenantGuard;
    private final MqttIngestService mqttIngestService;

    public DeviceService(
        DeviceRepository deviceRepository,
        LevelReadingRepository readingRepository,
        VendorRepository vendorRepository,
        TenantGuard tenantGuard,
        MqttIngestService mqttIngestService
    ) {
        this.deviceRepository = deviceRepository;
        this.readingRepository = readingRepository;
        this.vendorRepository = vendorRepository;
        this.tenantGuard = tenantGuard;
        this.mqttIngestService = mqttIngestService;
    }

    public List<DeviceSummary> listForCurrentVendor() {
        AuthPrincipal p = tenantGuard.current();
        UUID vendorId = p.isSuperAdmin()
            ? null
            : p.vendorId();
        List<Device> devices = vendorId == null
            ? deviceRepository.findAll()
            : deviceRepository.findByVendorIdOrderByDisplayNameAsc(vendorId);
        return devices.stream().map(this::toSummary).toList();
    }

    @Transactional
    public DeviceSummary register(UUID vendorId, String deviceTag, String displayName, String chipId) {
        tenantGuard.requireVendorScope(vendorId);
        if (deviceRepository.existsByDeviceTag(deviceTag)) {
            throw new IllegalArgumentException("Device tag already registered");
        }
        Vendor vendor = vendorRepository.findById(vendorId)
            .orElseThrow(() -> new IllegalArgumentException("Vendor not found"));
        Device d = new Device();
        d.setVendor(vendor);
        d.setDeviceTag(deviceTag);
        d.setDisplayName(displayName);
        d.setChipId(chipId);
        return toSummary(deviceRepository.save(d));
    }

    public PageResponse<ReadingPoint> readings(UUID deviceId, int page, int size) {
        Device device = deviceRepository.findById(deviceId)
            .orElseThrow(() -> new IllegalArgumentException("Device not found"));
        if (!tenantGuard.current().isSuperAdmin()) {
            tenantGuard.requireVendorScope(device.getVendor().getId());
        }
        var result = readingRepository.findByDeviceIdOrderByReceivedAtDesc(
            deviceId, PageRequest.of(page, size));
        List<ReadingPoint> items = result.getContent().stream()
            .map(r -> new ReadingPoint(
                r.getId(), r.getReceivedAt(), r.getPercentFilled(),
                r.getVolumeLiters(), r.getTemperatureC(), r.getPayloadJson()))
            .toList();
        return new PageResponse<>(items, result.getTotalElements(), page, size);
    }

    public List<ReadingPoint> reportRange(UUID vendorId, Instant from, Instant to) {
        UUID scoped = tenantGuard.requireVendorScope(vendorId);
        return readingRepository.findVendorReadingsInRange(scoped, from, to).stream()
            .map(r -> new ReadingPoint(
                r.getId(), r.getReceivedAt(), r.getPercentFilled(),
                r.getVolumeLiters(), r.getTemperatureC(), r.getPayloadJson()))
            .toList();
    }

    public void sendCommand(UUID deviceId, String command) throws Exception {
        Device device = deviceRepository.findById(deviceId)
            .orElseThrow(() -> new IllegalArgumentException("Device not found"));
        AuthPrincipal p = tenantGuard.current();
        if (!p.isSuperAdmin()) {
            tenantGuard.requireVendorScope(device.getVendor().getId());
        }
        if (p.role() == UserRole.VIEWER) {
            throw new TenantGuard.AccessDeniedException("Viewers cannot send commands");
        }
        String json = "{\"command\":\"" + command + "\"}";
        mqttIngestService.publishCommand(device.getDeviceTag(), device.getTopicPrefix(), "command", json);
    }

    private DeviceSummary toSummary(Device d) {
        LevelReading latest = readingRepository.findFirstByDeviceIdOrderByReceivedAtDesc(d.getId());
        Long uptimeMs = d.getUptimeMs();
        return new DeviceSummary(
            d.getId(),
            d.getDeviceTag(),
            d.getDisplayName(),
            d.isOnline(),
            d.getLastSeenAt(),
            latest != null ? latest.getPercentFilled() : null,
            latest != null ? latest.getVolumeLiters() : null,
            latest != null ? latest.getReceivedAt() : null,
            formatUptime(uptimeMs),
            uptimeMs
        );
    }

    /** Format millis the same way as ESP TimeManager::getUptimeString(). */
    static String formatUptime(Long uptimeMs) {
        if (uptimeMs == null || uptimeMs < 0) return null;
        long totalSec = uptimeMs / 1000L;
        long days = totalSec / 86400L;
        long hours = (totalSec % 86400L) / 3600L;
        long mins = (totalSec % 3600L) / 60L;
        long secs = totalSec % 60L;
        StringBuilder sb = new StringBuilder();
        if (days > 0) sb.append(days).append('d').append(' ');
        if (days > 0 || hours > 0) sb.append(hours).append('h').append(' ');
        if (days > 0 || hours > 0 || mins > 0) sb.append(mins).append('m').append(' ');
        sb.append(secs).append('s');
        return sb.toString().trim();
    }
}
