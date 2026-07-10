package com.flm.platform.admin.web;

import com.flm.platform.admin.service.AuditService;
import com.flm.platform.admin.service.DatabaseAdminService;
import com.flm.platform.security.AuthPrincipal;
import com.flm.platform.security.RequiresStepUp;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.security.core.annotation.AuthenticationPrincipal;
import org.springframework.web.bind.annotation.*;
import java.util.List;
import java.util.Map;

@RestController
@RequestMapping("/api/admin/platform/database")
public class PlatformDatabaseController {

    private final DatabaseAdminService databaseAdminService;
    private final AuditService auditService;

    public PlatformDatabaseController(DatabaseAdminService databaseAdminService, AuditService auditService) {
        this.databaseAdminService = databaseAdminService;
        this.auditService = auditService;
    }

    @GetMapping("/status")
    @PreAuthorize("hasAuthority('PLATFORM_DB_READ')")
    public Map<String, Object> status() {
        return databaseAdminService.status();
    }

    @GetMapping("/backups")
    @PreAuthorize("hasAuthority('PLATFORM_DB_READ')")
    public List<Map<String, Object>> backups() {
        return databaseAdminService.listBackups();
    }

    @PostMapping("/backup")
    @PreAuthorize("hasAuthority('PLATFORM_DB_BACKUP')")
    public Map<String, String> backup(@AuthenticationPrincipal AuthPrincipal actor) {
        String name = databaseAdminService.createBackup();
        auditService.log(actor, "DB_BACKUP", "database", Map.of("file", name));
        return Map.of("filename", name);
    }

    @PostMapping("/restore")
    @PreAuthorize("hasAuthority('PLATFORM_DB_RESTORE')")
    @RequiresStepUp
    public void restore(
        @AuthenticationPrincipal AuthPrincipal actor,
        @RequestBody Map<String, Object> body
    ) {
        String filename = (String) body.get("filename");
        boolean confirm = Boolean.TRUE.equals(body.get("confirm"));
        databaseAdminService.restoreBackup(filename, confirm);
        auditService.log(actor, "DB_RESTORE", "database", Map.of("file", filename));
    }
}
