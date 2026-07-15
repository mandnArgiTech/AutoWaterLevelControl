package com.flm.platform.admin.service;

import com.flm.platform.domain.SavedQuery;
import com.flm.platform.domain.SavedQueryRepository;
import com.flm.platform.security.AuthPrincipal;
import net.sf.jsqlparser.parser.CCJSqlParserUtil;
import net.sf.jsqlparser.statement.Statement;
import net.sf.jsqlparser.statement.delete.Delete;
import net.sf.jsqlparser.statement.drop.Drop;
import net.sf.jsqlparser.statement.insert.Insert;
import net.sf.jsqlparser.statement.select.Select;
import net.sf.jsqlparser.statement.truncate.Truncate;
import net.sf.jsqlparser.statement.update.Update;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import javax.sql.DataSource;
import java.sql.*;
import java.util.*;
import java.util.regex.Pattern;

@Service
public class SqlWorkspaceService {

    public enum SqlClass { READ, WRITE, DDL, DESTRUCTIVE }

    private final DataSource dataSource;
    private final SavedQueryRepository savedQueryRepository;
    private final AuditService auditService;
    private final int maxRows;
    private final int queryTimeoutSeconds;

    private static final Pattern DESTRUCTIVE = Pattern.compile(
        "(?i)\\b(DROP|TRUNCATE|ALTER|CREATE|GRANT|REVOKE)\\b");

    public SqlWorkspaceService(
        DataSource dataSource,
        SavedQueryRepository savedQueryRepository,
        AuditService auditService,
        @Value("${flm.platform.sql.max-rows:500}") int maxRows,
        @Value("${flm.platform.sql.query-timeout-seconds:30}") int queryTimeoutSeconds
    ) {
        this.dataSource = dataSource;
        this.savedQueryRepository = savedQueryRepository;
        this.auditService = auditService;
        this.maxRows = maxRows;
        this.queryTimeoutSeconds = queryTimeoutSeconds;
    }

    public SqlClass classify(String sql) {
        String trimmed = sql.trim();
        if (DESTRUCTIVE.matcher(trimmed).find()) {
            return SqlClass.DESTRUCTIVE;
        }
        try {
            Statement stmt = CCJSqlParserUtil.parse(trimmed);
            if (stmt instanceof Select) return SqlClass.READ;
            if (stmt instanceof Insert || stmt instanceof Update || stmt instanceof Delete) return SqlClass.WRITE;
            if (stmt instanceof Drop || stmt instanceof Truncate) return SqlClass.DESTRUCTIVE;
            return SqlClass.DDL;
        } catch (Exception e) {
            if (trimmed.toUpperCase().startsWith("SELECT")) return SqlClass.READ;
            return SqlClass.DESTRUCTIVE;
        }
    }

    public Map<String, Object> execute(AuthPrincipal actor, String sql, boolean confirmDestructive, String confirmationPhrase) {
        String normalized = normalizeSql(sql);
        if (normalized.isBlank()) {
            throw new IllegalArgumentException("SQL is empty");
        }

        // Trailing ';' is common in editors; Postgres JDBC rejects it. Allow only single-statement.
        if (normalized.contains(";")) {
            throw new IllegalArgumentException("Only a single SQL statement is allowed");
        }

        SqlClass cls = classify(normalized);
        if (cls == SqlClass.DESTRUCTIVE || cls == SqlClass.DDL) {
            if (!confirmDestructive || !"EXECUTE".equals(confirmationPhrase)) {
                throw new IllegalArgumentException("Destructive/DDL statements require confirmDestructive and confirmationPhrase=EXECUTE");
            }
        }

        long start = System.currentTimeMillis();
        Map<String, Object> result = new LinkedHashMap<>();
        result.put("classification", cls.name());

        try (Connection conn = dataSource.getConnection();
             java.sql.Statement jdbcStmt = conn.createStatement()) {
            jdbcStmt.setQueryTimeout(queryTimeoutSeconds);
            jdbcStmt.setMaxRows(maxRows);

            if (cls == SqlClass.READ) {
                try (ResultSet rs = jdbcStmt.executeQuery(normalized)) {
                    ResultSetMetaData meta = rs.getMetaData();
                    int cols = meta.getColumnCount();
                    List<String> columns = new ArrayList<>();
                    for (int i = 1; i <= cols; i++) columns.add(meta.getColumnLabel(i));
                    List<Map<String, Object>> rows = new ArrayList<>();
                    int count = 0;
                    while (rs.next() && count < maxRows) {
                        Map<String, Object> row = new LinkedHashMap<>();
                        for (int i = 1; i <= cols; i++) {
                            Object val = rs.getObject(i);
                            if (val instanceof java.util.UUID u) {
                                val = u.toString();
                            } else if (val instanceof Timestamp ts) {
                                val = ts.toInstant().toString();
                            }
                            row.put(columns.get(i - 1), val);
                        }
                        rows.add(row);
                        count++;
                    }
                    result.put("columns", columns);
                    result.put("rows", rows);
                    result.put("rowCount", count);
                    result.put("truncated", count >= maxRows);
                }
            } else {
                int updated = jdbcStmt.executeUpdate(normalized);
                result.put("updatedRows", updated);
            }
            result.put("durationMs", System.currentTimeMillis() - start);
            result.put("success", true);
            auditService.log(actor, "SQL_EXECUTE", "database", Map.of(
                "classification", cls.name(),
                "sqlHash", Integer.toHexString(normalized.hashCode()),
                "durationMs", result.get("durationMs")
            ));
            return result;
        } catch (IllegalArgumentException e) {
            throw e;
        } catch (Exception e) {
            auditService.log(actor, "SQL_EXECUTE_FAILED", "database", Map.of(
                "error", e.getMessage() != null ? e.getMessage() : "unknown",
                "classification", cls.name()
            ));
            throw new IllegalStateException("SQL execution failed: " + e.getMessage(), e);
        }
    }

    /** Strip trailing semicolons / whitespace so editor-style SQL works with Postgres JDBC. */
    static String normalizeSql(String sql) {
        if (sql == null) return "";
        String t = sql.trim();
        while (t.endsWith(";")) {
            t = t.substring(0, t.length() - 1).trim();
        }
        return t;
    }

    @Transactional(readOnly = true)
    public List<SavedQuery> savedQueries(UUID userId) {
        return savedQueryRepository.findByUserIdOrderByCreatedAtDesc(userId);
    }

    @Transactional
    public SavedQuery saveQuery(UUID userId, String name, String sql) {
        SavedQuery q = new SavedQuery();
        q.setUserId(userId);
        q.setName(name);
        q.setSqlText(sql);
        return savedQueryRepository.save(q);
    }

    public List<Map<String, String>> schema() {
        JdbcTemplate jdbc = new JdbcTemplate(dataSource);
        return jdbc.query(
            "SELECT table_name, column_name, data_type FROM information_schema.columns " +
            "WHERE table_schema = 'public' ORDER BY table_name, ordinal_position",
            (rs, rowNum) -> {
                Map<String, String> m = new LinkedHashMap<>();
                m.put("table", rs.getString("table_name"));
                m.put("column", rs.getString("column_name"));
                m.put("type", rs.getString("data_type"));
                return m;
            }
        );
    }
}
