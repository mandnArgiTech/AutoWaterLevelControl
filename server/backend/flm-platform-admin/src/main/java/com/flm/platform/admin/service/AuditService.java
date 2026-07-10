package com.flm.platform.admin.service;

import com.flm.platform.domain.PlatformAuditLog;
import com.flm.platform.domain.PlatformAuditLogRepository;
import com.flm.platform.security.AuthPrincipal;
import jakarta.servlet.http.HttpServletRequest;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.Pageable;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.web.context.request.RequestContextHolder;
import org.springframework.web.context.request.ServletRequestAttributes;
import java.util.Map;
import java.util.UUID;

@Service
public class AuditService {

    private final PlatformAuditLogRepository repository;

    public AuditService(PlatformAuditLogRepository repository) {
        this.repository = repository;
    }

    @Transactional
    public void log(AuthPrincipal actor, String action, String resource, Map<String, Object> detail) {
        PlatformAuditLog entry = new PlatformAuditLog();
        entry.setActorUserId(actor != null ? actor.userId() : null);
        entry.setAction(action);
        entry.setResource(resource);
        entry.setDetail(detail);
        HttpServletRequest req = currentRequest();
        if (req != null) {
            entry.setIpAddress(clientIp(req));
            entry.setUserAgent(req.getHeader("User-Agent"));
        }
        repository.save(entry);
    }

    @Transactional
    public void logSystem(String action, String resource, Map<String, Object> detail) {
        log(null, action, resource, detail);
    }

    @Transactional(readOnly = true)
    public Page<PlatformAuditLog> list(Pageable pageable) {
        return repository.findAllByOrderByCreatedAtDesc(pageable);
    }

    private HttpServletRequest currentRequest() {
        var attrs = RequestContextHolder.getRequestAttributes();
        if (attrs instanceof ServletRequestAttributes sra) {
            return sra.getRequest();
        }
        return null;
    }

    private String clientIp(HttpServletRequest req) {
        String forwarded = req.getHeader("X-Forwarded-For");
        if (forwarded != null && !forwarded.isBlank()) {
            return forwarded.split(",")[0].trim();
        }
        return req.getRemoteAddr();
    }
}
