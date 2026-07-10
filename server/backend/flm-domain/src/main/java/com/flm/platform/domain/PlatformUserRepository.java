package com.flm.platform.domain;

import org.springframework.data.jpa.repository.JpaRepository;
import java.util.List;
import java.util.Optional;
import java.util.UUID;

public interface PlatformUserRepository extends JpaRepository<PlatformUser, UUID> {
    Optional<PlatformUser> findByEmailIgnoreCase(String email);
    List<PlatformUser> findByVendorId(UUID vendorId);
    boolean existsByEmailIgnoreCase(String email);
}
