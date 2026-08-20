package com.flm.platform.api;

import com.flm.platform.api.dto.DeviceModelSummary;
import com.flm.platform.api.service.DeviceModelService;
import org.springframework.web.bind.annotation.*;
import java.util.List;

@RestController
@RequestMapping("/api/device-models")
public class DeviceModelController {

    private final DeviceModelService deviceModelService;

    public DeviceModelController(DeviceModelService deviceModelService) {
        this.deviceModelService = deviceModelService;
    }

    @GetMapping
    public List<DeviceModelSummary> list() {
        return deviceModelService.list();
    }

    @GetMapping("/{modelKey}")
    public DeviceModelSummary get(@PathVariable String modelKey) {
        return deviceModelService.getByKey(modelKey);
    }
}
