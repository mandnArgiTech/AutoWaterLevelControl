package com.flm.platform.mqtt.telemetry;

import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Repository;

import java.sql.Timestamp;
import java.time.Instant;
import java.util.UUID;

/**
 * Upserts the latest node health snapshot ({@code device_health_latest}).
 */
@Repository
public class DeviceHealthDao {

    private final JdbcTemplate jdbc;

    public DeviceHealthDao(JdbcTemplate jdbc) {
        this.jdbc = jdbc;
    }

    public void upsert(
        UUID deviceId,
        Instant receivedAt,
        Integer freeHeap,
        Integer minFreeHeap,
        Integer maxFreeBlock,
        Short cpuPercent,
        Short rssi,
        String ipAddress,
        String macAddress,
        Long uptimeMs,
        String firmware
    ) {
        jdbc.update("""
            INSERT INTO device_health_latest (
                device_id, received_at, free_heap, min_free_heap, max_free_block,
                cpu_percent, rssi, ip_address, mac_address, uptime_ms, firmware
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT (device_id) DO UPDATE SET
                received_at = EXCLUDED.received_at,
                free_heap = COALESCE(EXCLUDED.free_heap, device_health_latest.free_heap),
                min_free_heap = COALESCE(EXCLUDED.min_free_heap, device_health_latest.min_free_heap),
                max_free_block = COALESCE(EXCLUDED.max_free_block, device_health_latest.max_free_block),
                cpu_percent = COALESCE(EXCLUDED.cpu_percent, device_health_latest.cpu_percent),
                rssi = COALESCE(EXCLUDED.rssi, device_health_latest.rssi),
                ip_address = COALESCE(EXCLUDED.ip_address, device_health_latest.ip_address),
                mac_address = COALESCE(EXCLUDED.mac_address, device_health_latest.mac_address),
                uptime_ms = COALESCE(EXCLUDED.uptime_ms, device_health_latest.uptime_ms),
                firmware = COALESCE(EXCLUDED.firmware, device_health_latest.firmware)
            """,
            deviceId,
            Timestamp.from(receivedAt),
            freeHeap,
            minFreeHeap,
            maxFreeBlock,
            cpuPercent,
            rssi,
            blankToNull(ipAddress),
            blankToNull(macAddress),
            uptimeMs,
            blankToNull(firmware)
        );
    }

    private static String blankToNull(String s) {
        return (s == null || s.isBlank()) ? null : s;
    }
}
