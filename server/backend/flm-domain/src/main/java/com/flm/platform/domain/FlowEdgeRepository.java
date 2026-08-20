package com.flm.platform.domain;

import org.springframework.data.jpa.repository.JpaRepository;
import java.util.List;
import java.util.UUID;

public interface FlowEdgeRepository extends JpaRepository<FlowEdge, UUID> {
    List<FlowEdge> findBySiteIdOrderByCreatedAtAsc(UUID siteId);
}
