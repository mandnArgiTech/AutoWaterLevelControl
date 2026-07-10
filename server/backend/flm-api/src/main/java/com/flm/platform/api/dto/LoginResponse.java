package com.flm.platform.api.dto;

import com.flm.platform.common.UserRole;
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
    boolean mustChangePassword
) {}
