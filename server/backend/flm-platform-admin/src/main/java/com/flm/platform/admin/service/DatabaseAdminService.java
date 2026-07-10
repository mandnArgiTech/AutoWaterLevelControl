package com.flm.platform.admin.service;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Service;
import javax.sql.DataSource;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.nio.file.*;
import java.time.Instant;
import java.time.format.DateTimeFormatter;
import java.util.*;
import java.util.stream.Collectors;

@Service
public class DatabaseAdminService {

    private final JdbcTemplate jdbc;
    private final String serverRoot;
    private final String postgresComposeFile;
    private final String backupDir;

    public DatabaseAdminService(
        DataSource dataSource,
        @Value("${flm.platform.server-root:.}") String serverRoot,
        @Value("${flm.platform.postgres-compose-file:docker-compose.postgres.yml}") String postgresComposeFile,
        @Value("${flm.platform.backup-dir:backups}") String backupDir
    ) {
        this.jdbc = new JdbcTemplate(dataSource);
        this.serverRoot = serverRoot;
        this.postgresComposeFile = postgresComposeFile;
        this.backupDir = backupDir;
    }

    public Map<String, Object> status() {
        Map<String, Object> status = new LinkedHashMap<>();
        status.put("ready", Boolean.TRUE.equals(jdbc.queryForObject("SELECT true", Boolean.class)));
        status.put("databaseSize", jdbc.queryForObject(
            "SELECT pg_size_pretty(pg_database_size(current_database()))", String.class));
        status.put("connections", jdbc.queryForObject(
            "SELECT count(*) FROM pg_stat_activity WHERE datname = current_database()", Integer.class));
        status.put("tables", jdbc.queryForList(
            "SELECT relname AS name, pg_size_pretty(pg_total_relation_size(relid)) AS size " +
            "FROM pg_catalog.pg_statio_user_tables ORDER BY pg_total_relation_size(relid) DESC LIMIT 20"));
        return status;
    }

    public List<Map<String, Object>> listBackups() {
        Path dir = Paths.get(serverRoot, backupDir);
        if (!Files.exists(dir)) {
            return List.of();
        }
        try {
            return Files.list(dir)
                .filter(p -> p.toString().endsWith(".sql") || p.toString().endsWith(".sql.gz"))
                .map(p -> {
                    try {
                        Map<String, Object> m = new LinkedHashMap<>();
                        m.put("name", p.getFileName().toString());
                        m.put("size", Files.size(p));
                        m.put("modifiedAt", Files.getLastModifiedTime(p).toInstant().toString());
                        return m;
                    } catch (Exception e) {
                        return Map.<String, Object>of("name", p.getFileName().toString());
                    }
                })
                .collect(Collectors.toList());
        } catch (Exception e) {
            return List.of();
        }
    }

    public String createBackup() {
        String ts = DateTimeFormatter.ofPattern("yyyyMMdd-HHmmss").format(Instant.now().atZone(java.time.ZoneOffset.UTC));
        String filename = "flm-" + ts + ".sql.gz";
        Path dir = Paths.get(serverRoot, backupDir);
        try {
            Files.createDirectories(dir);
            Path out = dir.resolve(filename);
            ProcessBuilder pb = new ProcessBuilder(
                "docker", "compose", "-f", postgresComposeFile, "exec", "-T", "postgres",
                "pg_dump", "-U", envOr("POSTGRES_USER", "flmAdmin"), envOr("POSTGRES_DB", "flmDB")
            );
            pb.directory(Paths.get(serverRoot).toFile());
            pb.redirectErrorStream(true);
            Process proc = pb.start();
            try (var gzipOut = new java.util.zip.GZIPOutputStream(Files.newOutputStream(out));
                 var reader = new BufferedReader(new InputStreamReader(proc.getInputStream()))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    gzipOut.write((line + "\n").getBytes());
                }
            }
            int code = proc.waitFor();
            if (code != 0) {
                Files.deleteIfExists(out);
                throw new IllegalStateException("pg_dump failed with code " + code);
            }
            return filename;
        } catch (Exception e) {
            throw new IllegalStateException("Backup failed: " + e.getMessage(), e);
        }
    }

    public void restoreBackup(String filename, boolean confirm) {
        if (!confirm) {
            throw new IllegalArgumentException("Restore requires confirm=true");
        }
        Path file = Paths.get(serverRoot, backupDir, filename);
        if (!Files.exists(file)) {
            throw new IllegalArgumentException("Backup not found");
        }
        try {
            ProcessBuilder pb = new ProcessBuilder(
                "docker", "compose", "-f", postgresComposeFile, "exec", "-T", "postgres",
                "psql", "-U", envOr("POSTGRES_USER", "flmAdmin"), "-d", envOr("POSTGRES_DB", "flmDB")
            );
            pb.directory(Paths.get(serverRoot).toFile());
            pb.redirectInput(file.toFile());
            Process proc = pb.start();
            int code = proc.waitFor();
            if (code != 0) {
                throw new IllegalStateException("Restore failed with code " + code);
            }
        } catch (Exception e) {
            throw new IllegalStateException("Restore failed: " + e.getMessage(), e);
        }
    }

    private String envOr(String key, String def) {
        String v = System.getenv(key);
        return v != null && !v.isBlank() ? v : def;
    }
}
