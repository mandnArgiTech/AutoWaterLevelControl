package com.flm.platform.mqtt.telemetry;

import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Component;
import java.sql.Timestamp;
import java.sql.Types;
import java.time.Instant;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Bounded queue + flush loop: batch INSERT into level_readings and upsert device_latest.
 */
@Component
public class ReadingBatchWriter {

    private static final Logger log = LoggerFactory.getLogger(ReadingBatchWriter.class);

    private static final String INSERT_READING = """
        INSERT INTO level_readings (
            device_id, received_at, percent_filled, volume_liters,
            water_height_mm, distance_mm, temperature_c, humidity_pct,
            battery_mv, battery_percent
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT (device_id, received_at) DO UPDATE SET
            percent_filled = EXCLUDED.percent_filled,
            volume_liters = EXCLUDED.volume_liters,
            water_height_mm = EXCLUDED.water_height_mm,
            distance_mm = EXCLUDED.distance_mm,
            temperature_c = EXCLUDED.temperature_c,
            humidity_pct = EXCLUDED.humidity_pct,
            battery_mv = EXCLUDED.battery_mv,
            battery_percent = EXCLUDED.battery_percent
        """;

    private static final String UPSERT_LATEST = """
        INSERT INTO device_latest (
            device_id, received_at, percent_filled, volume_liters,
            water_height_mm, distance_mm, temperature_c, humidity_pct,
            battery_mv, battery_percent, uptime_ms, rssi, online
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT (device_id) DO UPDATE SET
            received_at = EXCLUDED.received_at,
            percent_filled = COALESCE(EXCLUDED.percent_filled, device_latest.percent_filled),
            volume_liters = COALESCE(EXCLUDED.volume_liters, device_latest.volume_liters),
            water_height_mm = COALESCE(EXCLUDED.water_height_mm, device_latest.water_height_mm),
            distance_mm = COALESCE(EXCLUDED.distance_mm, device_latest.distance_mm),
            temperature_c = COALESCE(EXCLUDED.temperature_c, device_latest.temperature_c),
            humidity_pct = COALESCE(EXCLUDED.humidity_pct, device_latest.humidity_pct),
            battery_mv = COALESCE(EXCLUDED.battery_mv, device_latest.battery_mv),
            battery_percent = COALESCE(EXCLUDED.battery_percent, device_latest.battery_percent),
            uptime_ms = COALESCE(EXCLUDED.uptime_ms, device_latest.uptime_ms),
            rssi = COALESCE(EXCLUDED.rssi, device_latest.rssi),
            online = EXCLUDED.online
        """;

    private static final String UPDATE_DEVICE = """
        UPDATE devices SET
            online = ?,
            last_seen_at = ?,
            uptime_ms = COALESCE(?, uptime_ms)
        WHERE id = ?
        """;

    private final JdbcTemplate jdbc;
    private final int batchSize;
    private final long flushIntervalMs;
    private final int queueCapacity;
    private final BlockingQueue<TelemetrySample> queue;
    private final AtomicBoolean running = new AtomicBoolean(false);
    private Thread worker;

    public ReadingBatchWriter(
        JdbcTemplate jdbc,
        @Value("${flm.telemetry.batch-size:1000}") int batchSize,
        @Value("${flm.telemetry.flush-interval-ms:500}") long flushIntervalMs,
        @Value("${flm.telemetry.queue-capacity:50000}") int queueCapacity
    ) {
        this.jdbc = jdbc;
        this.batchSize = Math.max(1, batchSize);
        this.flushIntervalMs = Math.max(50, flushIntervalMs);
        this.queueCapacity = Math.max(batchSize, queueCapacity);
        this.queue = new ArrayBlockingQueue<>(this.queueCapacity);
    }

    @PostConstruct
    public void start() {
        if (!running.compareAndSet(false, true)) return;
        worker = new Thread(this::runLoop, "telemetry-batch-writer");
        worker.setDaemon(true);
        worker.start();
        log.info("Telemetry batch writer started batchSize={} flushIntervalMs={} queueCapacity={}",
            batchSize, flushIntervalMs, queueCapacity);
    }

    @PreDestroy
    public void stop() {
        running.set(false);
        if (worker != null) {
            worker.interrupt();
            try {
                worker.join(3000);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
        flushRemaining();
    }

    /** Non-blocking offer; drops oldest on overflow to protect the MQTT thread. */
    public boolean enqueue(TelemetrySample sample) {
        if (sample == null) return false;
        if (queue.offer(sample)) return true;
        queue.poll();
        return queue.offer(sample);
    }

    private void runLoop() {
        List<TelemetrySample> buf = new ArrayList<>(batchSize);
        while (running.get()) {
            try {
                TelemetrySample first = queue.poll(flushIntervalMs, TimeUnit.MILLISECONDS);
                if (first != null) {
                    buf.add(first);
                    queue.drainTo(buf, batchSize - 1);
                }
                if (!buf.isEmpty()) {
                    flush(buf);
                    buf.clear();
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                break;
            } catch (Exception e) {
                log.error("Telemetry batch flush failed: {}", e.getMessage(), e);
                buf.clear();
            }
        }
        flushRemaining();
    }

    private void flushRemaining() {
        List<TelemetrySample> buf = new ArrayList<>(batchSize);
        queue.drainTo(buf);
        while (!buf.isEmpty()) {
            try {
                flush(buf);
            } catch (Exception e) {
                log.error("Final telemetry flush failed: {}", e.getMessage());
            }
            buf.clear();
            queue.drainTo(buf);
        }
    }

    private void flush(List<TelemetrySample> samples) {
        List<TelemetrySample> levels = samples.stream().filter(TelemetrySample::isLevelReading).toList();
        if (!levels.isEmpty()) {
            jdbc.batchUpdate(INSERT_READING, levels, levels.size(), (ps, s) -> bindReading(ps, s));
        }

        // Latest wins per device within the batch
        Map<UUID, TelemetrySample> latestByDevice = new LinkedHashMap<>();
        for (TelemetrySample s : samples) {
            latestByDevice.merge(s.deviceId(), s, (a, b) ->
                a.receivedAt().isAfter(b.receivedAt()) ? a : b);
        }
        List<TelemetrySample> latest = new ArrayList<>(latestByDevice.values());
        jdbc.batchUpdate(UPSERT_LATEST, latest, latest.size(), (ps, s) -> bindLatest(ps, s));
        jdbc.batchUpdate(UPDATE_DEVICE, latest, latest.size(), (ps, s) -> {
            ps.setBoolean(1, s.online());
            ps.setTimestamp(2, Timestamp.from(s.receivedAt()));
            if (s.uptimeMs() != null) {
                ps.setLong(3, s.uptimeMs());
            } else {
                ps.setNull(3, Types.BIGINT);
            }
            ps.setObject(4, s.deviceId());
        });
        log.debug("Flushed telemetry levels={} latest={}", levels.size(), latest.size());
    }

    private static void bindReading(java.sql.PreparedStatement ps, TelemetrySample s) throws java.sql.SQLException {
        ps.setObject(1, s.deviceId());
        ps.setTimestamp(2, Timestamp.from(s.receivedAt() != null ? s.receivedAt() : Instant.now()));
        setFloat(ps, 3, s.percentFilled());
        setFloat(ps, 4, s.volumeLiters());
        setShort(ps, 5, s.waterHeightMm());
        setShort(ps, 6, s.distanceMm());
        setFloat(ps, 7, s.temperatureC());
        setShort(ps, 8, s.humidityPct());
        setShort(ps, 9, s.batteryMv());
        setShort(ps, 10, s.batteryPercent());
    }

    private static void bindLatest(java.sql.PreparedStatement ps, TelemetrySample s) throws java.sql.SQLException {
        ps.setObject(1, s.deviceId());
        ps.setTimestamp(2, Timestamp.from(s.receivedAt() != null ? s.receivedAt() : Instant.now()));
        setFloat(ps, 3, s.percentFilled());
        setFloat(ps, 4, s.volumeLiters());
        setShort(ps, 5, s.waterHeightMm());
        setShort(ps, 6, s.distanceMm());
        setFloat(ps, 7, s.temperatureC());
        setShort(ps, 8, s.humidityPct());
        setShort(ps, 9, s.batteryMv());
        setShort(ps, 10, s.batteryPercent());
        if (s.uptimeMs() != null) {
            ps.setLong(11, s.uptimeMs());
        } else {
            ps.setNull(11, Types.BIGINT);
        }
        setShort(ps, 12, s.rssi());
        ps.setBoolean(13, s.online());
    }

    private static void setFloat(java.sql.PreparedStatement ps, int i, Float v) throws java.sql.SQLException {
        if (v == null) ps.setNull(i, Types.REAL);
        else ps.setFloat(i, v);
    }

    private static void setShort(java.sql.PreparedStatement ps, int i, Short v) throws java.sql.SQLException {
        if (v == null) ps.setNull(i, Types.SMALLINT);
        else ps.setShort(i, v);
    }
}
