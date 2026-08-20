package com.flm.platform.api;

import com.flm.platform.api.dto.CapabilitySummary;
import com.flm.platform.api.service.CapabilityService;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import java.util.List;

@RestController
@RequestMapping("/api/capabilities")
public class CapabilityController {

    private final CapabilityService capabilityService;

    public CapabilityController(CapabilityService capabilityService) {
        this.capabilityService = capabilityService;
    }

    @GetMapping
    public List<CapabilitySummary> list() {
        return capabilityService.list();
    }
}
