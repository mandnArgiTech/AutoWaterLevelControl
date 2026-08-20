package com.flm.platform.mqtt.telemetry;

import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.jdbc.core.RowMapper;
import org.springframework.stereotype.Repository;
import java.sql.Timestamp;
import java.time.Instant;
import java.util.List;
import java.util.Optional;
import java.util.UUID;

@Repository
public class LevelReadingDao {

    public record DeviceWithLatest(
        UUID id,
        String deviceTag,
        String displayName,
        boolean online,
        Instant lastSeenAt,
        Long uptimeMs,
        Float percentFilled,
        Float volumeLiters,
        Instant latestReadingAt,
        String lifecycleState,
        String modelKey,
        UUID siteId,
        Integer freeHeap,
        Integer minFreeHeap,
        Integer maxFreeBlock,
        Short rssi,
        String ipAddress,
        String firmware,
        Instant healthReceivedAt
    ) {}

    public record ReadingRow(
        Instant receivedAt,
        Float percentFilled,
        Float volumeLiters,
        Float temperatureC
    ) {}

    private static final String DEVICE_SELECT = """
        SELECT d.id, d.device_tag, d.display_name, d.online, d.last_seen_at, d.uptime_ms,
               d.lifecycle_state, d.site_id,
               m.model_key,
               l.percent_filled, l.volume_liters, l.received_at AS latest_received_at,
               h.free_heap, h.min_free_heap, h.max_free_block, h.rssi AS health_rssi,
               h.ip_address, h.firmware, h.received_at AS health_received_at
        FROM devices d
        LEFT JOIN device_latest l ON l.device_id = d.id
        LEFT JOIN device_model m ON m.id = d.model_id
        LEFT JOIN device_health_latest h ON h.device_id = d.id
        """;

    private static final RowMapper<DeviceWithLatest> DEVICE_MAPPER = (rs, i) -> new DeviceWithLatest(
        (UUID) rs.getObject("id"),
        rs.getString("device_tag"),
        rs.getString("display_name"),
        rs.getBoolean("online"),
        toInstant(rs.getTimestamp("last_seen_at")),
        rs.getObject("uptime_ms") != null ? rs.getLong("uptime_ms") : null,
        rs.getObject("percent_filled") != null ? rs.getFloat("percent_filled") : null,
        rs.getObject("volume_liters") != null ? rs.getFloat("volume_liters") : null,
        toInstant(rs.getTimestamp("latest_received_at")),
        rs.getString("lifecycle_state"),
        rs.getString("model_key"),
        (UUID) rs.getObject("site_id"),
        rs.getObject("free_heap") != null ? rs.getInt("free_heap") : null,
        rs.getObject("min_free_heap") != null ? rs.getInt("min_free_heap") : null,
        rs.getObject("max_free_block") != null ? rs.getInt("max_free_block") : null,
        rs.getObject("health_rssi") != null ? rs.getShort("health_rssi") : null,
        rs.getString("ip_address"),
        rs.getString("firmware"),
        toInstant(rs.getTimestamp("health_received_at"))
    );

    private static final RowMapper<ReadingRow> READING_MAPPER = (rs, i) -> new ReadingRow(
        toInstant(rs.getTimestamp("received_at")),
        rs.getObject("percent_filled") != null ? rs.getFloat("percent_filled") : null,
        rs.getObject("volume_liters") != null ? rs.getFloat("volume_liters") : null,
        rs.getObject("temperature_c") != null ? rs.getFloat("temperature_c") : null
    );

    private final JdbcTemplate jdbc;

    public LevelReadingDao(JdbcTemplate jdbc) {
        this.jdbc = jdbc;
    }

    public List<DeviceWithLatest> listDevicesWithLatest(UUID vendorId) {
        if (vendorId == null) {
            return jdbc.query(DEVICE_SELECT + " ORDER BY d.display_name ASC", DEVICE_MAPPER);
        }
        return jdbc.query(DEVICE_SELECT + " WHERE d.vendor_id = ? ORDER BY d.display_name ASC",
            DEVICE_MAPPER, vendorId);
    }

    public Optional<DeviceWithLatest> findDeviceWithLatest(UUID deviceId) {
        List<DeviceWithLatest> rows = jdbc.query(
            DEVICE_SELECT + " WHERE d.id = ?", DEVICE_MAPPER, deviceId);
        return rows.stream().findFirst();
    }

    public List<ReadingRow> findByDeviceDesc(UUID deviceId, int limit, int offset) {
        return jdbc.query("""
            SELECT received_at, percent_filled, volume_liters, temperature_c
            FROM level_readings
            WHERE device_id = ?
            ORDER BY received_at DESC
            LIMIT ? OFFSET ?
            """, READING_MAPPER, deviceId, limit, offset);
    }

    public long countByDevice(UUID deviceId) {
        Long n = jdbc.queryForObject(
            "SELECT COUNT(*) FROM level_readings WHERE device_id = ?",
            Long.class, deviceId);
        return n != null ? n : 0L;
    }

    public List<ReadingRow> findVendorRange(UUID vendorId, Instant from, Instant to) {
        return jdbc.query("""
            SELECT r.received_at, r.percent_filled, r.volume_liters, r.temperature_c
            FROM level_readings r
            INNER JOIN devices d ON d.id = r.device_id
            WHERE d.vendor_id = ?
              AND r.received_at >= ?
              AND r.received_at < ?
            ORDER BY r.received_at DESC
            """, READING_MAPPER, vendorId, Timestamp.from(from), Timestamp.from(to));
    }

    private static Instant toInstant(Timestamp ts) {
        return ts != null ? ts.toInstant() : null;
    }
}
