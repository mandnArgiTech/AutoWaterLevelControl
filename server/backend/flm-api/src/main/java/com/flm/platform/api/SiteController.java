package com.flm.platform.api;

import com.flm.platform.api.dto.SiteSummary;
import com.flm.platform.api.service.SiteService;
import org.springframework.web.bind.annotation.*;
import java.util.Map;
import java.util.UUID;

@RestController
@RequestMapping("/api/sites")
public class SiteController {

    private final SiteService siteService;

    public SiteController(SiteService siteService) {
        this.siteService = siteService;
    }

    @GetMapping
    public java.util.List<SiteSummary> list() {
        return siteService.list();
    }

    @GetMapping("/{siteId}")
    public SiteSummary get(@PathVariable UUID siteId) {
        return siteService.get(siteId);
    }

    @PostMapping
    public SiteSummary create(@RequestBody Map<String, Object> body) {
        return siteService.create(body);
    }

    @PutMapping("/{siteId}")
    public SiteSummary update(@PathVariable UUID siteId, @RequestBody Map<String, Object> body) {
        return siteService.update(siteId, body);
    }

    @DeleteMapping("/{siteId}")
    public Map<String, String> delete(@PathVariable UUID siteId) {
        siteService.delete(siteId);
        return Map.of("status", "deleted");
    }
}
