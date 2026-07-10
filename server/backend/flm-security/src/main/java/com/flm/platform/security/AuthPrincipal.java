package com.flm.platform.security;

import com.flm.platform.common.UserRole;
import java.util.List;
import java.util.UUID;

/** Authenticated principal stored in SecurityContext after JWT validation. */
public record AuthPrincipal(
    UUID userId,
    String email,
    UserRole role,
    UUID vendorId,
    String vendorCode,
    List<String> permissions,
    boolean stepUpValid
) {
    public AuthPrincipal(UUID userId, String email, UserRole role, UUID vendorId, String vendorCode) {
        this(userId, email, role, vendorId, vendorCode, List.of(), false);
    }

    public boolean isSuperAdmin() {
        return role == UserRole.SUPER_ADMIN;
    }

    public boolean canManageUsers() {
        return hasPermission("USER_MANAGE");
    }

    public boolean hasPermission(String permission) {
        return permissions != null && permissions.contains(permission);
    }
}
