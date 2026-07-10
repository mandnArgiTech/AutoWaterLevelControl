package com.flm.platform.domain;

import org.springframework.data.jpa.repository.JpaRepository;
import java.util.List;
import java.util.UUID;

public interface SavedQueryRepository extends JpaRepository<SavedQuery, UUID> {
    List<SavedQuery> findByUserIdOrderByCreatedAtDesc(UUID userId);
}
