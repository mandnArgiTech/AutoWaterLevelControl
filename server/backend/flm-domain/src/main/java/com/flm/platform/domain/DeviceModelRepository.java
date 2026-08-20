package com.flm.platform.domain;

import org.springframework.data.jpa.repository.JpaRepository;
import java.util.Optional;
import java.util.UUID;

public interface DeviceModelRepository extends JpaRepository<DeviceModel, UUID> {
    Optional<DeviceModel> findByModelKey(String modelKey);
    boolean existsByModelKey(String modelKey);
}
