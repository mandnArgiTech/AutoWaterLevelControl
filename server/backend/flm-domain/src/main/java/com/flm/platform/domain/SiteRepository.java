package com.flm.platform.domain;

import org.springframework.data.jpa.repository.JpaRepository;
import java.util.List;
import java.util.UUID;

public interface SiteRepository extends JpaRepository<Site, UUID> {
    List<Site> findByVendorIdOrderByNameAsc(UUID vendorId);
    List<Site> findAllByOrderByNameAsc();
}
