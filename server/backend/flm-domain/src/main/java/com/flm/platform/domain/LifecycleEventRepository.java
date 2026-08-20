package com.flm.platform.domain;

import org.springframework.data.jpa.repository.JpaRepository;
import java.util.List;
import java.util.UUID;

public interface LifecycleEventRepository extends JpaRepository<LifecycleEvent, UUID> {
    List<LifecycleEvent> findByDeviceIdOrderByCreatedAtDesc(UUID deviceId);
}
