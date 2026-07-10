package com.flm.platform.admin.web;

import com.flm.platform.common.UserRole;
import java.util.UUID;

public record PlatformUserSummary(
    UUID id,
    String email,
    String displayName,
    UserRole role,
    boolean active,
    boolean mustChangePassword
) {}
