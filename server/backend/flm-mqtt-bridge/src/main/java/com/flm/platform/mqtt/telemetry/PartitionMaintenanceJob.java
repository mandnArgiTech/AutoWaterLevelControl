package com.flm.platform.mqtt.telemetry;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;
import java.time.LocalDate;
import java.time.ZoneOffset;
import java.time.temporal.TemporalAdjusters;
import java.util.List;
import java.util.Map;

/**
 * Creates upcoming monthly partitions and drops ones older than retention.
 */
@Component
public class PartitionMaintenanceJob {

    private static final Logger log = LoggerFactory.getLogger(PartitionMaintenanceJob.class);

    private final JdbcTemplate jdbc;
    private final int retentionMonths;
    private final int preCreateMonths;

    public PartitionMaintenanceJob(
        JdbcTemplate jdbc,
        @Value("${flm.telemetry.retention-months:60}") int retentionMonths,
        @Value("${flm.telemetry.precreate-months:3}") int preCreateMonths
    ) {
        this.jdbc = jdbc;
        this.retentionMonths = Math.max(1, retentionMonths);
        this.preCreateMonths = Math.max(1, preCreateMonths);
    }

    /** Daily at 01:15 UTC */
    @Scheduled(cron = "0 15 1 * * *", zone = "UTC")
    public void maintain() {
        LocalDate month = LocalDate.now(ZoneOffset.UTC).with(TemporalAdjusters.firstDayOfMonth());
        for (int i = 0; i <= preCreateMonths; i++) {
            LocalDate m = month.plusMonths(i);
            ensurePartition(m.getYear(), m.getMonthValue());
        }
        LocalDate dropBefore = month.minusMonths(retentionMonths);
        dropOldPartitions(dropBefore);
    }

    public void ensurePartition(int year, int month) {
        try {
            jdbc.query(
                "SELECT create_level_readings_partition(?, ?)",
                (rs, rowNum) -> null,
                year, month
            );
            try {
                jdbc.query("SELECT create_telemetry_env_partition(?, ?)", (rs, rowNum) -> null, year, month);
                jdbc.query("SELECT create_telemetry_soil_partition(?, ?)", (rs, rowNum) -> null, year, month);
            } catch (Exception ignored) {
                // functions may not exist on older DBs
            }
            log.debug("Ensured partition {}-{}", year, month);
        } catch (Exception e) {
            log.warn("Could not create partition {}-{}: {}", year, month, e.getMessage());
        }
    }

    private void dropOldPartitions(LocalDate dropBefore) {
        String prefix = "level_readings_";
        List<Map<String, Object>> parts = jdbc.queryForList("""
            SELECT c.relname AS name
            FROM pg_inherits i
            JOIN pg_class c ON c.oid = i.inhrelid
            JOIN pg_class p ON p.oid = i.inhparent
            WHERE p.relname = 'level_readings'
            """);
        for (Map<String, Object> row : parts) {
            String name = String.valueOf(row.get("name"));
            if (!name.startsWith(prefix)) continue;
            String yyyyMm = name.substring(prefix.length()); // YYYY_MM
            String[] bits = yyyyMm.split("_");
            if (bits.length != 2) continue;
            try {
                int y = Integer.parseInt(bits[0]);
                int m = Integer.parseInt(bits[1]);
                LocalDate partMonth = LocalDate.of(y, m, 1);
                if (partMonth.isBefore(dropBefore) && name.matches("level_readings_\\d{4}_\\d{2}")) {
                    jdbc.execute("DROP TABLE IF EXISTS " + name);
                    log.info("Dropped telemetry partition {} (retention={} months)", name, retentionMonths);
                }
            } catch (NumberFormatException ignored) {
                // skip oddly named tables
            }
        }
    }
}
