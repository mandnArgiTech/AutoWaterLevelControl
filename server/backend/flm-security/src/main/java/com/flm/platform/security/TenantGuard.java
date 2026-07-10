package com.flm.platform.security;

import com.flm.platform.common.UserRole;
import org.springframework.security.core.Authentication;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.stereotype.Component;
import java.util.UUID;

@Component
public class TenantGuard {

    public AuthPrincipal current() {
        Authentication auth = SecurityContextHolder.getContext().getAuthentication();
        if (auth == null || !(auth.getPrincipal() instanceof AuthPrincipal p)) {
            throw new AccessDeniedException("Not authenticated");
        }
        return p;
    }

    public UUID requireVendorScope(UUID requestedVendorId) {
        AuthPrincipal p = current();
        if (p.isSuperAdmin()) {
            return requestedVendorId;
        }
        if (p.vendorId() == null || !p.vendorId().equals(requestedVendorId)) {
            throw new AccessDeniedException("Vendor scope denied");
        }
        return p.vendorId();
    }

    public UUID vendorIdOrNull() {
        return current().vendorId();
    }

    public static class AccessDeniedException extends RuntimeException {
        public AccessDeniedException(String message) { super(message); }
    }
}
