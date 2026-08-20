package com.flm.platform.api;

import com.flm.platform.api.dto.*;
import com.flm.platform.api.service.TopologyService;
import org.springframework.web.bind.annotation.*;
import java.util.List;
import java.util.Map;
import java.util.UUID;

@RestController
@RequestMapping("/api")
public class TopologyController {

    private final TopologyService topologyService;

    public TopologyController(TopologyService topologyService) {
        this.topologyService = topologyService;
    }

    @GetMapping("/sites/{siteId}/assets")
    public List<AssetSummary> listAssets(@PathVariable UUID siteId) {
        return topologyService.listAssets(siteId);
    }

    @PostMapping("/sites/{siteId}/assets")
    public AssetSummary createAsset(@PathVariable UUID siteId, @RequestBody Map<String, Object> body) {
        return topologyService.createAsset(siteId, body);
    }

    @DeleteMapping("/sites/{siteId}/assets/{assetId}")
    public Map<String, String> deleteAsset(@PathVariable UUID siteId, @PathVariable UUID assetId) {
        topologyService.deleteAsset(siteId, assetId);
        return Map.of("status", "deleted");
    }

    @GetMapping("/sites/{siteId}/flows")
    public List<FlowEdgeSummary> listFlows(@PathVariable UUID siteId) {
        return topologyService.listFlows(siteId);
    }

    @PostMapping("/sites/{siteId}/flows")
    public FlowEdgeSummary createFlow(@PathVariable UUID siteId, @RequestBody Map<String, Object> body) {
        return topologyService.createFlow(siteId, body);
    }

    @DeleteMapping("/sites/{siteId}/flows/{flowId}")
    public Map<String, String> deleteFlow(@PathVariable UUID siteId, @PathVariable UUID flowId) {
        topologyService.deleteFlow(siteId, flowId);
        return Map.of("status", "deleted");
    }

    @PostMapping("/sites/{siteId}/provision")
    public Map<String, Object> provision(@PathVariable UUID siteId, @RequestBody Map<String, Object> body) {
        return topologyService.provision(siteId, body);
    }

    @GetMapping("/assets/{assetId}/bindings")
    public List<BindingSummary> listBindings(@PathVariable UUID assetId) {
        return topologyService.listBindings(assetId);
    }

    @PostMapping("/bindings")
    public BindingSummary createBinding(@RequestBody Map<String, Object> body) {
        return topologyService.createBinding(body);
    }

    @PostMapping("/bindings/{bindingId}/end")
    public BindingSummary endBinding(@PathVariable UUID bindingId) {
        return topologyService.endBinding(bindingId);
    }

    @GetMapping("/devices/{deviceId}/channels")
    public List<Map<String, Object>> listChannels(@PathVariable UUID deviceId) {
        return topologyService.listChannels(deviceId);
    }

    @PostMapping("/devices/{deviceId}/channels")
    public Map<String, Object> ensureChannel(@PathVariable UUID deviceId, @RequestBody Map<String, Object> body) {
        String key = String.valueOf(body.get("capabilityKey"));
        short chan = body.get("chanIndex") != null
            ? Short.parseShort(String.valueOf(body.get("chanIndex"))) : 0;
        String name = body.get("name") != null ? String.valueOf(body.get("name")) : null;
        return topologyService.ensureChannel(deviceId, key, chan, name);
    }
}
