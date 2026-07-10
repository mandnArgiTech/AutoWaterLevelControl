package com.flm.platform.domain;

import org.springframework.data.domain.Page;
import org.springframework.data.domain.Pageable;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;
import java.time.Instant;
import java.util.List;
import java.util.UUID;

public interface LevelReadingRepository extends JpaRepository<LevelReading, UUID> {

    Page<LevelReading> findByDeviceIdOrderByReceivedAtDesc(UUID deviceId, Pageable pageable);

    @Query("""
        SELECT r FROM LevelReading r
        WHERE r.device.vendor.id = :vendorId
          AND r.receivedAt >= :from AND r.receivedAt < :to
        ORDER BY r.receivedAt DESC
        """)
    List<LevelReading> findVendorReadingsInRange(
        @Param("vendorId") UUID vendorId,
        @Param("from") Instant from,
        @Param("to") Instant to
    );

    LevelReading findFirstByDeviceIdOrderByReceivedAtDesc(UUID deviceId);
}
