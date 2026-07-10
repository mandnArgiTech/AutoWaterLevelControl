package com.flm.platform.security;

import com.flm.platform.common.UserRole;
import java.util.UUID;

/** Authenticated principal stored in SecurityContext after JWT validation. */
public record AuthPrincipal(
    UUID userId,
    String email,
    UserRole role,
    UUID vendorId,
    String vendorCode
) {
    public boolean isSuperAdmin() {
        return role == UserRole.SUPER_ADMIN;
    }

    public boolean canManageUsers() {
        return role == UserRole.SUPER_ADMIN || role == UserRole.VENDOR_ADMIN;
    }
}
