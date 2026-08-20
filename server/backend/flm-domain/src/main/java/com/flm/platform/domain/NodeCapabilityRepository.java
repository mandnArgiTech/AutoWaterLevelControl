package com.flm.platform.domain;

import org.springframework.data.jpa.repository.JpaRepository;
import java.util.List;
import java.util.UUID;

public interface NodeCapabilityRepository extends JpaRepository<NodeCapability, UUID> {
    List<NodeCapability> findByDeviceIdOrderByChanIndexAsc(UUID deviceId);
}
