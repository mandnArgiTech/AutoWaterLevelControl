package com.flm.platform.admin.web;

import com.flm.platform.admin.service.SqlWorkspaceService;
import com.flm.platform.domain.SavedQuery;
import com.flm.platform.security.AuthPrincipal;
import com.flm.platform.security.RequiresStepUp;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.security.core.annotation.AuthenticationPrincipal;
import org.springframework.web.bind.annotation.*;
import java.util.List;
import java.util.Map;

@RestController
@RequestMapping("/api/admin/platform/sql")
public class PlatformSqlController {

    private final SqlWorkspaceService sqlWorkspaceService;

    public PlatformSqlController(SqlWorkspaceService sqlWorkspaceService) {
        this.sqlWorkspaceService = sqlWorkspaceService;
    }

    @GetMapping("/schema")
    @PreAuthorize("hasAuthority('PLATFORM_SQL_READ')")
    public List<Map<String, String>> schema() {
        return sqlWorkspaceService.schema();
    }

    @PostMapping("/execute")
    @PreAuthorize("hasAuthority('PLATFORM_SQL_READ')")
    public Map<String, Object> execute(
        @AuthenticationPrincipal AuthPrincipal actor,
        @RequestBody SqlExecuteRequest req
    ) {
        SqlWorkspaceService.SqlClass cls = sqlWorkspaceService.classify(req.sql());
        if (cls != SqlWorkspaceService.SqlClass.READ) {
            // write path requires write permission + step-up handled by separate endpoint
            throw new IllegalArgumentException("Use /execute-write for non-read statements");
        }
        return sqlWorkspaceService.execute(actor, req.sql(), false, null);
    }

    @PostMapping("/execute-write")
    @PreAuthorize("hasAuthority('PLATFORM_SQL_WRITE')")
    @RequiresStepUp
    public Map<String, Object> executeWrite(
        @AuthenticationPrincipal AuthPrincipal actor,
        @RequestBody SqlExecuteRequest req
    ) {
        return sqlWorkspaceService.execute(
            actor, req.sql(), req.confirmDestructive(), req.confirmationPhrase());
    }

    @GetMapping("/saved")
    @PreAuthorize("hasAuthority('PLATFORM_SQL_READ')")
    public List<SavedQuery> saved(@AuthenticationPrincipal AuthPrincipal actor) {
        return sqlWorkspaceService.savedQueries(actor.userId());
    }

    @PostMapping("/saved")
    @PreAuthorize("hasAuthority('PLATFORM_SQL_READ')")
    public SavedQuery save(
        @AuthenticationPrincipal AuthPrincipal actor,
        @RequestBody Map<String, String> body
    ) {
        return sqlWorkspaceService.saveQuery(actor.userId(), body.get("name"), body.get("sql"));
    }

    public record SqlExecuteRequest(
        String sql,
        Boolean confirmDestructive,
        String confirmationPhrase
    ) {}
}
