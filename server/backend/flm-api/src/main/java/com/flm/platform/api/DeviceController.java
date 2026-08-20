package com.flm.platform.api;

import com.flm.platform.api.dto.DeviceSummary;
import com.flm.platform.common.PageResponse;
import com.flm.platform.api.dto.ReadingPoint;
import com.flm.platform.api.service.DeviceService;
import com.flm.platform.mqtt.telemetry.PendingDeviceDao;
import org.springframework.format.annotation.DateTimeFormat;
import org.springframework.web.bind.annotation.*;
import java.time.Instant;
import java.util.List;
import java.util.Map;
import java.util.UUID;

@RestController
@RequestMapping("/api/devices")
public class DeviceController {

    private final DeviceService deviceService;

    public DeviceController(DeviceService deviceService) {
        this.deviceService = deviceService;
    }

    @GetMapping
    public List<DeviceSummary> list() {
        return deviceService.listForCurrentVendor();
    }

    /** MQTT tags seen but not yet registered (approve via POST /api/devices). */
    @GetMapping("/pending")
    public List<PendingDeviceDao.PendingDevice> pending() {
        return deviceService.listPending();
    }

    @GetMapping("/{deviceId}")
    public DeviceSummary get(@PathVariable UUID deviceId) {
        return deviceService.get(deviceId);
    }

    @PostMapping
    public DeviceSummary register(@RequestBody Map<String, String> body) {
        UUID siteId = null;
        if (body.get("siteId") != null && !body.get("siteId").isBlank()) {
            siteId = UUID.fromString(body.get("siteId"));
        }
        return deviceService.register(
            UUID.fromString(body.get("vendorId")),
            body.get("deviceTag"),
            body.get("displayName"),
            body.get("chipId"),
            body.get("modelKey"),
            siteId,
            body.get("commType")
        );
    }

    @PostMapping("/{deviceId}/lifecycle")
    public Map<String, Object> lifecycle(@PathVariable UUID deviceId, @RequestBody Map<String, String> body) {
        return deviceService.transitionLifecycle(deviceId, body.get("toState"), body.get("reason"));
    }

    @GetMapping("/{deviceId}/readings")
    public PageResponse<ReadingPoint> readings(
        @PathVariable UUID deviceId,
        @RequestParam(defaultValue = "0") int page,
        @RequestParam(defaultValue = "50") int size
    ) {
        return deviceService.readings(deviceId, page, size);
    }

    @PostMapping("/{deviceId}/command")
    public Map<String, String> command(@PathVariable UUID deviceId, @RequestBody Map<String, String> body) throws Exception {
        return deviceService.sendCommand(deviceId, body.get("command"));
    }
}

@RestController
@RequestMapping("/api/reports")
class ReportController {

    private final DeviceService deviceService;

    ReportController(DeviceService deviceService) {
        this.deviceService = deviceService;
    }

    @GetMapping("/vendor/{vendorId}")
    public List<ReadingPoint> vendorReport(
        @PathVariable UUID vendorId,
        @RequestParam @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME) Instant from,
        @RequestParam @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME) Instant to
    ) {
        return deviceService.reportRange(vendorId, from, to);
    }
}
