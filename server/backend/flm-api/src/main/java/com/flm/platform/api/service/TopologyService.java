package com.flm.platform.api.service;

import com.flm.platform.api.dto.*;
import com.flm.platform.domain.*;
import com.flm.platform.security.TenantGuard;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import java.time.Instant;
import java.util.*;

@Service
public class TopologyService {

    private final SiteRepository siteRepository;
    private final AssetRepository assetRepository;
    private final FlowEdgeRepository flowEdgeRepository;
    private final BindingRepository bindingRepository;
    private final NodeCapabilityRepository nodeCapabilityRepository;
    private final CapabilityRepository capabilityRepository;
    private final DeviceRepository deviceRepository;
    private final TenantGuard tenantGuard;

    public TopologyService(
        SiteRepository siteRepository,
        AssetRepository assetRepository,
        FlowEdgeRepository flowEdgeRepository,
        BindingRepository bindingRepository,
        NodeCapabilityRepository nodeCapabilityRepository,
        CapabilityRepository capabilityRepository,
        DeviceRepository deviceRepository,
        TenantGuard tenantGuard
    ) {
        this.siteRepository = siteRepository;
        this.assetRepository = assetRepository;
        this.flowEdgeRepository = flowEdgeRepository;
        this.bindingRepository = bindingRepository;
        this.nodeCapabilityRepository = nodeCapabilityRepository;
        this.capabilityRepository = capabilityRepository;
        this.deviceRepository = deviceRepository;
        this.tenantGuard = tenantGuard;
    }

    private Site requireSite(UUID siteId) {
        Site site = siteRepository.findById(siteId)
            .orElseThrow(() -> new IllegalArgumentException("Site not found"));
        if (!tenantGuard.current().isSuperAdmin()) {
            tenantGuard.requireVendorScope(site.getVendor().getId());
        }
        return site;
    }

    public List<AssetSummary> listAssets(UUID siteId) {
        requireSite(siteId);
        return assetRepository.findBySiteIdOrderByNameAsc(siteId).stream().map(this::toAsset).toList();
    }

    @Transactional
    public AssetSummary createAsset(UUID siteId, Map<String, Object> body) {
        Site site = requireSite(siteId);
        Asset a = new Asset();
        a.setSite(site);
        a.setName(String.valueOf(body.get("name")));
        a.setKind(String.valueOf(body.getOrDefault("kind", "GENERIC")));
        if (body.get("subtype") != null) a.setSubtype(String.valueOf(body.get("subtype")));
        if (body.get("capacityL") != null) a.setCapacityL(Double.valueOf(String.valueOf(body.get("capacityL"))));
        if (body.get("attributes") != null) a.setAttributes(String.valueOf(body.get("attributes")));
        return toAsset(assetRepository.save(a));
    }

    @Transactional
    public void deleteAsset(UUID siteId, UUID assetId) {
        requireSite(siteId);
        Asset a = assetRepository.findById(assetId)
            .orElseThrow(() -> new IllegalArgumentException("Asset not found"));
        if (!a.getSite().getId().equals(siteId)) {
            throw new IllegalArgumentException("Asset does not belong to site");
        }
        assetRepository.delete(a);
    }

    public List<FlowEdgeSummary> listFlows(UUID siteId) {
        requireSite(siteId);
        return flowEdgeRepository.findBySiteIdOrderByCreatedAtAsc(siteId).stream().map(this::toFlow).toList();
    }

    @Transactional
    public FlowEdgeSummary createFlow(UUID siteId, Map<String, Object> body) {
        Site site = requireSite(siteId);
        Asset from = assetRepository.findById(UUID.fromString(String.valueOf(body.get("fromAssetId"))))
            .orElseThrow(() -> new IllegalArgumentException("fromAssetId not found"));
        Asset to = assetRepository.findById(UUID.fromString(String.valueOf(body.get("toAssetId"))))
            .orElseThrow(() -> new IllegalArgumentException("toAssetId not found"));
        if (!from.getSite().getId().equals(siteId) || !to.getSite().getId().equals(siteId)) {
            throw new IllegalArgumentException("Assets must belong to the site");
        }
        FlowEdge e = new FlowEdge();
        e.setSite(site);
        e.setFromAsset(from);
        e.setToAsset(to);
        if (body.get("viaAssetId") != null) {
            e.setViaAsset(assetRepository.findById(UUID.fromString(String.valueOf(body.get("viaAssetId"))))
                .orElseThrow(() -> new IllegalArgumentException("viaAssetId not found")));
        }
        if (body.get("kind") != null) e.setKind(String.valueOf(body.get("kind")));
        return toFlow(flowEdgeRepository.save(e));
    }

    @Transactional
    public void deleteFlow(UUID siteId, UUID flowId) {
        requireSite(siteId);
        FlowEdge e = flowEdgeRepository.findById(flowId)
            .orElseThrow(() -> new IllegalArgumentException("Flow not found"));
        if (!e.getSite().getId().equals(siteId)) {
            throw new IllegalArgumentException("Flow does not belong to site");
        }
        flowEdgeRepository.delete(e);
    }

    public List<BindingSummary> listBindings(UUID assetId) {
        Asset asset = assetRepository.findById(assetId)
            .orElseThrow(() -> new IllegalArgumentException("Asset not found"));
        requireSite(asset.getSite().getId());
        return bindingRepository.findByAssetIdOrderByValidFromDesc(assetId).stream().map(this::toBinding).toList();
    }

    @Transactional
    public BindingSummary createBinding(Map<String, Object> body) {
        UUID assetId = UUID.fromString(String.valueOf(body.get("assetId")));
        UUID channelId = UUID.fromString(String.valueOf(body.get("nodeCapabilityId")));
        String role = String.valueOf(body.get("role")).trim().toUpperCase();
        if (!role.equals("MEASURES") && !role.equals("ACTUATES")) {
            throw new IllegalArgumentException("role must be MEASURES or ACTUATES");
        }
        Asset asset = assetRepository.findById(assetId)
            .orElseThrow(() -> new IllegalArgumentException("Asset not found"));
        requireSite(asset.getSite().getId());
        NodeCapability channel = nodeCapabilityRepository.findById(channelId)
            .orElseThrow(() -> new IllegalArgumentException("nodeCapabilityId not found"));

        if (bindingRepository.findByNodeCapabilityIdAndRoleAndValidToIsNull(channelId, role).isPresent()) {
            throw new IllegalArgumentException("Overlapping active binding for channel+role");
        }

        Binding b = new Binding();
        b.setAsset(asset);
        b.setNodeCapability(channel);
        b.setRole(role);
        return toBinding(bindingRepository.save(b));
    }

    @Transactional
    public BindingSummary endBinding(UUID bindingId) {
        Binding b = bindingRepository.findById(bindingId)
            .orElseThrow(() -> new IllegalArgumentException("Binding not found"));
        requireSite(b.getAsset().getSite().getId());
        b.setValidTo(Instant.now());
        return toBinding(bindingRepository.save(b));
    }

    /**
     * Ensure a node capability channel exists for a device (used by wizard / provision).
     */
    @Transactional
    public Map<String, Object> ensureChannel(UUID deviceId, String capabilityKey, short chanIndex, String name) {
        Device device = deviceRepository.findById(deviceId)
            .orElseThrow(() -> new IllegalArgumentException("Device not found"));
        if (!tenantGuard.current().isSuperAdmin()) {
            tenantGuard.requireVendorScope(device.getVendor().getId());
        }
        Capability cap = capabilityRepository.findByCapKey(capabilityKey)
            .orElseThrow(() -> new IllegalArgumentException("Unknown capability: " + capabilityKey));
        Optional<NodeCapability> existing = nodeCapabilityRepository.findByDeviceIdOrderByChanIndexAsc(deviceId).stream()
            .filter(nc -> nc.getCapability().getId().equals(cap.getId()) && nc.getChanIndex() == chanIndex)
            .findFirst();
        NodeCapability nc = existing.orElseGet(() -> {
            NodeCapability created = new NodeCapability();
            created.setDevice(device);
            created.setCapability(cap);
            created.setChanIndex(chanIndex);
            created.setName(name != null ? name : capabilityKey);
            return nodeCapabilityRepository.save(created);
        });
        return Map.of(
            "id", nc.getId().toString(),
            "deviceId", deviceId.toString(),
            "capabilityKey", capabilityKey,
            "chanIndex", (int) chanIndex
        );
    }

    @Transactional(readOnly = true)
    public List<Map<String, Object>> listChannels(UUID deviceId) {
        Device device = deviceRepository.findById(deviceId)
            .orElseThrow(() -> new IllegalArgumentException("Device not found"));
        if (!tenantGuard.current().isSuperAdmin()) {
            tenantGuard.requireVendorScope(device.getVendor().getId());
        }
        return nodeCapabilityRepository.findByDeviceIdOrderByChanIndexAsc(deviceId).stream()
            .map(nc -> {
                Map<String, Object> m = new LinkedHashMap<>();
                m.put("id", nc.getId());
                m.put("deviceId", deviceId);
                m.put("capabilityKey", nc.getCapability().getCapKey());
                m.put("chanIndex", nc.getChanIndex());
                m.put("name", nc.getName());
                return m;
            })
            .toList();
    }

    /**
     * One-shot Home provision: create municipal→sump→pump→overhead topology blob.
     */
    @Transactional
    @SuppressWarnings("unchecked")
    public Map<String, Object> provision(UUID siteId, Map<String, Object> body) {
        Site site = requireSite(siteId);
        Map<String, UUID> assetIds = new LinkedHashMap<>();
        List<Map<String, Object>> assetsIn = (List<Map<String, Object>>) body.getOrDefault("assets", List.of());
        for (Map<String, Object> a : assetsIn) {
            AssetSummary created = createAsset(siteId, a);
            String key = String.valueOf(a.getOrDefault("key", created.name()));
            assetIds.put(key, created.id());
        }
        List<Map<String, Object>> flowsIn = (List<Map<String, Object>>) body.getOrDefault("flows", List.of());
        List<FlowEdgeSummary> flows = new ArrayList<>();
        for (Map<String, Object> f : flowsIn) {
            Map<String, Object> mapped = new HashMap<>(f);
            mapped.put("fromAssetId", resolveAssetRef(assetIds, f.get("from")));
            mapped.put("toAssetId", resolveAssetRef(assetIds, f.get("to")));
            if (f.get("via") != null) {
                mapped.put("viaAssetId", resolveAssetRef(assetIds, f.get("via")));
            }
            flows.add(createFlow(siteId, mapped));
        }
        Map<String, Object> result = new LinkedHashMap<>();
        result.put("siteId", site.getId());
        result.put("assets", assetIds);
        result.put("flows", flows);
        result.put("status", "ok");
        return result;
    }

    private static String resolveAssetRef(Map<String, UUID> assetIds, Object ref) {
        String key = String.valueOf(ref);
        if (assetIds.containsKey(key)) return assetIds.get(key).toString();
        return key; // assume UUID string
    }

    private AssetSummary toAsset(Asset a) {
        return new AssetSummary(a.getId(), a.getSite().getId(), a.getName(), a.getKind(),
            a.getSubtype(), a.getCapacityL(), a.getAttributes());
    }

    private FlowEdgeSummary toFlow(FlowEdge e) {
        return new FlowEdgeSummary(
            e.getId(), e.getSite().getId(),
            e.getFromAsset().getId(), e.getToAsset().getId(),
            e.getViaAsset() != null ? e.getViaAsset().getId() : null,
            e.getKind()
        );
    }

    private BindingSummary toBinding(Binding b) {
        return new BindingSummary(
            b.getId(), b.getAsset().getId(), b.getNodeCapability().getId(),
            b.getRole(), b.getValidFrom(), b.getValidTo()
        );
    }
}
