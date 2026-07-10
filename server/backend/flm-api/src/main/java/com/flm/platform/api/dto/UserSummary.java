package com.flm.platform.api.dto;

import com.flm.platform.common.UserRole;
import java.util.UUID;

public record UserSummary(
    UUID id,
    String email,
    String displayName,
    UserRole role,
    boolean active,
    boolean mustChangePassword
) {}
