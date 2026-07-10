package com.flm.platform.api;

import com.flm.platform.api.dto.DeviceSummary;
import com.flm.platform.common.PageResponse;
import com.flm.platform.api.dto.ReadingPoint;
import com.flm.platform.api.service.DeviceService;
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

    @PostMapping
    public DeviceSummary register(@RequestBody Map<String, String> body) {
        return deviceService.register(
            UUID.fromString(body.get("vendorId")),
            body.get("deviceTag"),
            body.get("displayName"),
            body.get("chipId")
        );
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
        deviceService.sendCommand(deviceId, body.get("command"));
        return Map.of("status", "sent");
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
