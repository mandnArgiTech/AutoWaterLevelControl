package com.flm.platform.api.dto;

import com.flm.platform.common.UserRole;
import java.util.List;
import java.util.UUID;

public record LoginResponse(
    String token,
    UUID userId,
    String email,
    String displayName,
    UserRole role,
    UUID vendorId,
    String vendorCode,
    String vendorName,
    boolean mustChangePassword,
    List<String> permissions,
    boolean mfaRequired,
    boolean mfaEnrolled,
    String mfaProvisioningUri
) {
    public LoginResponse(
        String token,
        UUID userId,
        String email,
        String displayName,
        UserRole role,
        UUID vendorId,
        String vendorCode,
        String vendorName,
        boolean mustChangePassword
    ) {
        this(token, userId, email, displayName, role, vendorId, vendorCode, vendorName,
            mustChangePassword, List.of(), false, false, null);
    }
}
