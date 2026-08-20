package com.flm.platform.api.service;

import com.flm.platform.api.dto.SiteSummary;
import com.flm.platform.domain.Site;
import com.flm.platform.domain.SiteRepository;
import com.flm.platform.domain.Vendor;
import com.flm.platform.domain.VendorRepository;
import com.flm.platform.security.AuthPrincipal;
import com.flm.platform.security.TenantGuard;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import java.util.List;
import java.util.Map;
import java.util.UUID;

@Service
public class SiteService {

    private final SiteRepository siteRepository;
    private final VendorRepository vendorRepository;
    private final TenantGuard tenantGuard;

    public SiteService(SiteRepository siteRepository, VendorRepository vendorRepository, TenantGuard tenantGuard) {
        this.siteRepository = siteRepository;
        this.vendorRepository = vendorRepository;
        this.tenantGuard = tenantGuard;
    }

    public List<SiteSummary> list() {
        AuthPrincipal p = tenantGuard.current();
        List<Site> sites = p.isSuperAdmin()
            ? siteRepository.findAllByOrderByNameAsc()
            : siteRepository.findByVendorIdOrderByNameAsc(p.vendorId());
        return sites.stream().map(this::toSummary).toList();
    }

    public SiteSummary get(UUID siteId) {
        Site site = siteRepository.findById(siteId)
            .orElseThrow(() -> new IllegalArgumentException("Site not found"));
        if (!tenantGuard.current().isSuperAdmin()) {
            tenantGuard.requireVendorScope(site.getVendor().getId());
        }
        return toSummary(site);
    }

    @Transactional
    public SiteSummary create(Map<String, Object> body) {
        UUID vendorId = UUID.fromString(String.valueOf(body.get("vendorId")));
        tenantGuard.requireVendorScope(vendorId);
        Vendor vendor = vendorRepository.findById(vendorId)
            .orElseThrow(() -> new IllegalArgumentException("Vendor not found"));
        Site site = new Site();
        site.setVendor(vendor);
        site.setName(String.valueOf(body.get("name")));
        if (body.get("kind") != null) site.setKind(String.valueOf(body.get("kind")));
        if (body.get("latitude") != null) site.setLatitude(Double.valueOf(String.valueOf(body.get("latitude"))));
        if (body.get("longitude") != null) site.setLongitude(Double.valueOf(String.valueOf(body.get("longitude"))));
        if (body.get("address") != null) site.setAddress(String.valueOf(body.get("address")));
        return toSummary(siteRepository.save(site));
    }

    @Transactional
    public SiteSummary update(UUID siteId, Map<String, Object> body) {
        Site site = siteRepository.findById(siteId)
            .orElseThrow(() -> new IllegalArgumentException("Site not found"));
        if (!tenantGuard.current().isSuperAdmin()) {
            tenantGuard.requireVendorScope(site.getVendor().getId());
        }
        if (body.get("name") != null) site.setName(String.valueOf(body.get("name")));
        if (body.get("kind") != null) site.setKind(String.valueOf(body.get("kind")));
        if (body.containsKey("latitude")) {
            site.setLatitude(body.get("latitude") == null ? null : Double.valueOf(String.valueOf(body.get("latitude"))));
        }
        if (body.containsKey("longitude")) {
            site.setLongitude(body.get("longitude") == null ? null : Double.valueOf(String.valueOf(body.get("longitude"))));
        }
        if (body.containsKey("address")) {
            site.setAddress(body.get("address") == null ? null : String.valueOf(body.get("address")));
        }
        if (body.get("active") != null) site.setActive(Boolean.parseBoolean(String.valueOf(body.get("active"))));
        return toSummary(siteRepository.save(site));
    }

    @Transactional
    public void delete(UUID siteId) {
        Site site = siteRepository.findById(siteId)
            .orElseThrow(() -> new IllegalArgumentException("Site not found"));
        if (!tenantGuard.current().isSuperAdmin()) {
            tenantGuard.requireVendorScope(site.getVendor().getId());
        }
        siteRepository.delete(site);
    }

    private SiteSummary toSummary(Site s) {
        return new SiteSummary(
            s.getId(), s.getVendor().getId(), s.getName(), s.getKind(),
            s.getLatitude(), s.getLongitude(), s.getAddress(), s.isActive(), s.getCreatedAt()
        );
    }
}
