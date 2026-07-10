package com.flm.platform.api.dto;

import jakarta.validation.constraints.NotBlank;

public record LoginRequest(
    @NotBlank String username,
    @NotBlank String password,
    /** Optional vendor portal code — required for vendor users, omit for super admin */
    String vendorCode,
    /** TOTP code when MFA is enabled */
    String otpCode
) {}
