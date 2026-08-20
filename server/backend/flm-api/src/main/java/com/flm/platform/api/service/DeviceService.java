package com.flm.platform.api.service;

import com.flm.platform.api.dto.*;
import com.flm.platform.common.PageResponse;
import com.flm.platform.common.UserRole;
import com.flm.platform.domain.*;
import com.flm.platform.mqtt.MqttIngestService;
import com.flm.platform.mqtt.telemetry.LevelReadingDao;
import com.flm.platform.mqtt.telemetry.PendingDeviceDao;
import com.flm.platform.security.AuthPrincipal;
import com.flm.platform.security.TenantGuard;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import java.time.Instant;
import java.util.List;
import java.util.Map;
import java.util.UUID;

@Service
public class DeviceService {

    private final DeviceRepository deviceRepository;
    private final VendorRepository vendorRepository;
    private final DeviceModelRepository deviceModelRepository;
    private final SiteRepository siteRepository;
    private final LifecycleEventRepository lifecycleEventRepository;
    private final TenantGuard tenantGuard;
    private final MqttIngestService mqttIngestService;
    private final LevelReadingDao readingDao;
    private final PendingDeviceDao pendingDeviceDao;

    public DeviceService(
        DeviceRepository deviceRepository,
        VendorRepository vendorRepository,
        DeviceModelRepository deviceModelRepository,
        SiteRepository siteRepository,
        LifecycleEventRepository lifecycleEventRepository,
        TenantGuard tenantGuard,
        MqttIngestService mqttIngestService,
        LevelReadingDao readingDao,
        PendingDeviceDao pendingDeviceDao
    ) {
        this.deviceRepository = deviceRepository;
        this.vendorRepository = vendorRepository;
        this.deviceModelRepository = deviceModelRepository;
        this.siteRepository = siteRepository;
        this.lifecycleEventRepository = lifecycleEventRepository;
        this.tenantGuard = tenantGuard;
        this.mqttIngestService = mqttIngestService;
        this.readingDao = readingDao;
        this.pendingDeviceDao = pendingDeviceDao;
    }

    public List<DeviceSummary> listForCurrentVendor() {
        AuthPrincipal p = tenantGuard.current();
        UUID vendorId = p.isSuperAdmin() ? null : p.vendorId();
        return readingDao.listDevicesWithLatest(vendorId).stream()
            .map(this::toSummary)
            .toList();
    }

    /** Unknown MQTT tags waiting for registration (platform-wide). */
    public List<PendingDeviceDao.PendingDevice> listPending() {
        // Any authenticated operator can see pending tags so they can register them.
        tenantGuard.current();
        return pendingDeviceDao.listAll();
    }

    @Transactional
    public DeviceSummary register(UUID vendorId, String deviceTag, String displayName, String chipId) {
        return register(vendorId, deviceTag, displayName, chipId, null, null, null);
    }

    @Transactional
    public DeviceSummary register(
        UUID vendorId,
        String deviceTag,
        String displayName,
        String chipId,
        String modelKey,
        UUID siteId,
        String commType
    ) {
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
        d.setLifecycleState(com.flm.platform.common.LifecycleState.ACTIVE);
        if (modelKey != null && !modelKey.isBlank()) {
            d.setModel(deviceModelRepository.findByModelKey(modelKey)
                .orElseThrow(() -> new IllegalArgumentException("Unknown modelKey: " + modelKey)));
        }
        if (siteId != null) {
            Site site = siteRepository.findById(siteId)
                .orElseThrow(() -> new IllegalArgumentException("Site not found"));
            if (!site.getVendor().getId().equals(vendorId)) {
                throw new IllegalArgumentException("Site does not belong to vendor");
            }
            d.setSite(site);
        }
        if (commType != null && !commType.isBlank()) {
            d.setCommType(com.flm.platform.common.CommType.valueOf(commType.trim()));
        }
        Device saved = deviceRepository.save(d);
        return readingDao.findDeviceWithLatest(saved.getId())
            .map(this::toSummary)
            .orElseGet(() -> new DeviceSummary(
                saved.getId(), saved.getDeviceTag(), saved.getDisplayName(),
                false, null, null, null, null, null, null,
                saved.getLifecycleState() != null ? saved.getLifecycleState().name() : null,
                saved.getModel() != null ? saved.getModel().getModelKey() : null,
                saved.getSite() != null ? saved.getSite().getId() : null,
                null, null, null, null, null, null, null));
    }

    public DeviceSummary get(UUID deviceId) {
        Device device = deviceRepository.findById(deviceId)
            .orElseThrow(() -> new IllegalArgumentException("Device not found"));
        if (!tenantGuard.current().isSuperAdmin()) {
            tenantGuard.requireVendorScope(device.getVendor().getId());
        }
        return readingDao.findDeviceWithLatest(deviceId)
            .map(this::toSummary)
            .orElseThrow(() -> new IllegalArgumentException("Device not found"));
    }

    @Transactional
    public Map<String, Object> transitionLifecycle(UUID deviceId, String toState, String reason) {
        Device device = deviceRepository.findById(deviceId)
            .orElseThrow(() -> new IllegalArgumentException("Device not found"));
        AuthPrincipal p = tenantGuard.current();
        if (!p.isSuperAdmin()) {
            tenantGuard.requireVendorScope(device.getVendor().getId());
        }
        com.flm.platform.common.LifecycleState target =
            com.flm.platform.common.LifecycleState.valueOf(toState.trim().toUpperCase());
        com.flm.platform.common.LifecycleState from = device.getLifecycleState();
        if (!isAllowedTransition(from, target)) {
            throw new IllegalArgumentException("Illegal lifecycle transition: " + from + " -> " + target);
        }
        device.setLifecycleState(target);
        deviceRepository.save(device);

        LifecycleEvent ev = new LifecycleEvent();
        ev.setDeviceId(device.getId());
        ev.setFromState(from.name());
        ev.setToState(target.name());
        ev.setReason(reason);
        ev.setActor(p.email() != null ? p.email() : p.toString());
        lifecycleEventRepository.save(ev);

        return Map.of(
            "deviceId", device.getId().toString(),
            "from", from.name(),
            "to", target.name(),
            "status", "ok"
        );
    }

    private static boolean isAllowedTransition(
        com.flm.platform.common.LifecycleState from,
        com.flm.platform.common.LifecycleState to
    ) {
        if (from == to) return true;
        return switch (from) {
            case PROVISIONED -> to == com.flm.platform.common.LifecycleState.INSTALLED
                || to == com.flm.platform.common.LifecycleState.ACTIVE
                || to == com.flm.platform.common.LifecycleState.DECOMMISSIONED;
            case INSTALLED -> to == com.flm.platform.common.LifecycleState.ACTIVE
                || to == com.flm.platform.common.LifecycleState.DECOMMISSIONED;
            case ACTIVE -> to == com.flm.platform.common.LifecycleState.MAINTENANCE
                || to == com.flm.platform.common.LifecycleState.FAULT
                || to == com.flm.platform.common.LifecycleState.DECOMMISSIONED;
            case MAINTENANCE -> to == com.flm.platform.common.LifecycleState.ACTIVE
                || to == com.flm.platform.common.LifecycleState.FAULT
                || to == com.flm.platform.common.LifecycleState.DECOMMISSIONED;
            case FAULT -> to == com.flm.platform.common.LifecycleState.ACTIVE
                || to == com.flm.platform.common.LifecycleState.MAINTENANCE
                || to == com.flm.platform.common.LifecycleState.DECOMMISSIONED;
            case DECOMMISSIONED -> false;
        };
    }

    public PageResponse<ReadingPoint> readings(UUID deviceId, int page, int size) {
        Device device = deviceRepository.findById(deviceId)
            .orElseThrow(() -> new IllegalArgumentException("Device not found"));
        if (!tenantGuard.current().isSuperAdmin()) {
            tenantGuard.requireVendorScope(device.getVendor().getId());
        }
        int safeSize = Math.min(Math.max(size, 1), 500);
        int safePage = Math.max(page, 0);
        int offset = safePage * safeSize;
        List<ReadingPoint> items = readingDao.findByDeviceDesc(deviceId, safeSize, offset).stream()
            .map(this::toPoint)
            .toList();
        long total = readingDao.countByDevice(deviceId);
        return new PageResponse<>(items, total, safePage, safeSize);
    }

    public List<ReadingPoint> reportRange(UUID vendorId, Instant from, Instant to) {
        UUID scoped = tenantGuard.requireVendorScope(vendorId);
        return readingDao.findVendorRange(scoped, from, to).stream()
            .map(this::toPoint)
            .toList();
    }

    public Map<String, String> sendCommand(UUID deviceId, String command) throws Exception {
        Device device = deviceRepository.findById(deviceId)
            .orElseThrow(() -> new IllegalArgumentException("Device not found"));
        AuthPrincipal p = tenantGuard.current();
        if (!p.isSuperAdmin()) {
            tenantGuard.requireVendorScope(device.getVendor().getId());
        }
        if (p.role() == UserRole.VIEWER) {
            throw new TenantGuard.AccessDeniedException("Viewers cannot send commands");
        }
        if (command == null || command.isBlank()) {
            throw new IllegalArgumentException("command is required");
        }
        String cmd = command.trim();
        String json = "{\"command\":\"" + cmd.replace("\"", "") + "\"}";
        String topic = device.getDeviceTag() + "/" + device.getTopicPrefix() + "/command";
        mqttIngestService.publishCommand(device.getDeviceTag(), device.getTopicPrefix(), "command", json);
        return Map.of(
            "status", "sent",
            "command", cmd,
            "topic", topic,
            "deviceTag", device.getDeviceTag()
        );
    }

    private DeviceSummary toSummary(LevelReadingDao.DeviceWithLatest d) {
        Long uptimeMs = d.uptimeMs();
        return new DeviceSummary(
            d.id(),
            d.deviceTag(),
            d.displayName(),
            d.online(),
            d.lastSeenAt(),
            d.percentFilled() != null ? d.percentFilled().doubleValue() : null,
            d.volumeLiters() != null ? d.volumeLiters().doubleValue() : null,
            d.latestReadingAt(),
            formatUptime(uptimeMs),
            uptimeMs,
            d.lifecycleState(),
            d.modelKey(),
            d.siteId(),
            d.freeHeap(),
            d.minFreeHeap(),
            d.maxFreeBlock(),
            d.rssi(),
            d.ipAddress(),
            d.firmware(),
            d.healthReceivedAt()
        );
    }

    private ReadingPoint toPoint(LevelReadingDao.ReadingRow r) {
        return new ReadingPoint(
            r.receivedAt(),
            r.percentFilled() != null ? r.percentFilled().doubleValue() : null,
            r.volumeLiters() != null ? r.volumeLiters().doubleValue() : null,
            r.temperatureC() != null ? r.temperatureC().doubleValue() : null
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
