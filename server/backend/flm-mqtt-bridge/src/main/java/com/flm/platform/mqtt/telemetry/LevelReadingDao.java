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
        Instant latestReadingAt
    ) {}

    public record ReadingRow(
        Instant receivedAt,
        Float percentFilled,
        Float volumeLiters,
        Float temperatureC
    ) {}

    private static final RowMapper<DeviceWithLatest> DEVICE_MAPPER = (rs, i) -> new DeviceWithLatest(
        (UUID) rs.getObject("id"),
        rs.getString("device_tag"),
        rs.getString("display_name"),
        rs.getBoolean("online"),
        toInstant(rs.getTimestamp("last_seen_at")),
        rs.getObject("uptime_ms") != null ? rs.getLong("uptime_ms") : null,
        rs.getObject("percent_filled") != null ? rs.getFloat("percent_filled") : null,
        rs.getObject("volume_liters") != null ? rs.getFloat("volume_liters") : null,
        toInstant(rs.getTimestamp("latest_received_at"))
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
            return jdbc.query("""
                SELECT d.id, d.device_tag, d.display_name, d.online, d.last_seen_at, d.uptime_ms,
                       l.percent_filled, l.volume_liters, l.received_at AS latest_received_at
                FROM devices d
                LEFT JOIN device_latest l ON l.device_id = d.id
                ORDER BY d.display_name ASC
                """, DEVICE_MAPPER);
        }
        return jdbc.query("""
            SELECT d.id, d.device_tag, d.display_name, d.online, d.last_seen_at, d.uptime_ms,
                   l.percent_filled, l.volume_liters, l.received_at AS latest_received_at
            FROM devices d
            LEFT JOIN device_latest l ON l.device_id = d.id
            WHERE d.vendor_id = ?
            ORDER BY d.display_name ASC
            """, DEVICE_MAPPER, vendorId);
    }

    public Optional<DeviceWithLatest> findDeviceWithLatest(UUID deviceId) {
        List<DeviceWithLatest> rows = jdbc.query("""
            SELECT d.id, d.device_tag, d.display_name, d.online, d.last_seen_at, d.uptime_ms,
                   l.percent_filled, l.volume_liters, l.received_at AS latest_received_at
            FROM devices d
            LEFT JOIN device_latest l ON l.device_id = d.id
            WHERE d.id = ?
            """, DEVICE_MAPPER, deviceId);
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
