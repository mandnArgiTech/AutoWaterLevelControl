package com.flm.platform.mqtt.telemetry;

import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.jdbc.core.RowMapper;
import org.springframework.stereotype.Repository;

import java.sql.Timestamp;
import java.time.Instant;
import java.util.List;

/**
 * Upserts unknown MQTT device tags so operators can register them later.
 */
@Repository
public class PendingDeviceDao {

    public record PendingDevice(
        String deviceTag,
        String chipId,
        String modelKey,
        String topicPrefix,
        String sampleTopic,
        String samplePayload,
        long sampleCount,
        Instant firstSeenAt,
        Instant lastSeenAt
    ) {}

    private static final RowMapper<PendingDevice> MAPPER = (rs, i) -> new PendingDevice(
        rs.getString("device_tag"),
        rs.getString("chip_id"),
        rs.getString("model_key"),
        rs.getString("topic_prefix"),
        rs.getString("sample_topic"),
        rs.getString("sample_payload"),
        rs.getLong("sample_count"),
        rs.getTimestamp("first_seen_at").toInstant(),
        rs.getTimestamp("last_seen_at").toInstant()
    );

    private final JdbcTemplate jdbc;

    public PendingDeviceDao(JdbcTemplate jdbc) {
        this.jdbc = jdbc;
    }

    public void record(String deviceTag, String chipId, String topicPrefix,
                       String sampleTopic, String samplePayload) {
        record(deviceTag, chipId, null, topicPrefix, sampleTopic, samplePayload);
    }

    public void record(String deviceTag, String chipId, String modelKey, String topicPrefix,
                       String sampleTopic, String samplePayload) {
        if (deviceTag == null || deviceTag.isBlank()) return;
        String payload = samplePayload;
        if (payload != null && payload.length() > 2000) {
            payload = payload.substring(0, 2000);
        }
        Instant now = Instant.now();
        jdbc.update("""
            INSERT INTO pending_devices (
                device_tag, chip_id, model_key, topic_prefix, sample_topic, sample_payload,
                sample_count, first_seen_at, last_seen_at
            ) VALUES (?, ?, ?, ?, ?, ?, 1, ?, ?)
            ON CONFLICT (device_tag) DO UPDATE SET
                chip_id = COALESCE(EXCLUDED.chip_id, pending_devices.chip_id),
                model_key = COALESCE(EXCLUDED.model_key, pending_devices.model_key),
                topic_prefix = COALESCE(EXCLUDED.topic_prefix, pending_devices.topic_prefix),
                sample_topic = EXCLUDED.sample_topic,
                sample_payload = EXCLUDED.sample_payload,
                sample_count = pending_devices.sample_count + 1,
                last_seen_at = EXCLUDED.last_seen_at
            """,
            deviceTag,
            blankToNull(chipId),
            blankToNull(modelKey),
            blankToNull(topicPrefix),
            blankToNull(sampleTopic),
            payload,
            Timestamp.from(now),
            Timestamp.from(now)
        );
    }

    public List<PendingDevice> listAll() {
        return jdbc.query("""
            SELECT device_tag, chip_id, model_key, topic_prefix, sample_topic, sample_payload,
                   sample_count, first_seen_at, last_seen_at
            FROM pending_devices
            ORDER BY last_seen_at DESC
            """, MAPPER);
    }

    private static String blankToNull(String s) {
        return (s == null || s.isBlank()) ? null : s;
    }
}
