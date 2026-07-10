package com.flm.platform.admin.web;

import com.flm.platform.admin.service.AuditService;
import com.flm.platform.domain.PlatformAuditLog;
import com.flm.platform.security.AuthPrincipal;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.PageRequest;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.security.core.annotation.AuthenticationPrincipal;
import org.springframework.web.bind.annotation.*;
import java.util.Map;

@RestController
@RequestMapping("/api/admin/platform/audit")
@PreAuthorize("hasAuthority('AUDIT_READ')")
public class PlatformAuditController {

    private final AuditService auditService;

    public PlatformAuditController(AuditService auditService) {
        this.auditService = auditService;
    }

    @GetMapping
    public Page<PlatformAuditLog> list(
        @RequestParam(defaultValue = "0") int page,
        @RequestParam(defaultValue = "50") int size
    ) {
        return auditService.list(PageRequest.of(page, Math.min(size, 200)));
    }
}
