package com.flm.platform.domain;

import org.springframework.data.jpa.repository.JpaRepository;
import java.util.List;
import java.util.Optional;
import java.util.UUID;

public interface DeviceRepository extends JpaRepository<Device, UUID> {
    Optional<Device> findByDeviceTag(String deviceTag);
    List<Device> findByVendorIdOrderByDisplayNameAsc(UUID vendorId);
    boolean existsByDeviceTag(String deviceTag);
}
