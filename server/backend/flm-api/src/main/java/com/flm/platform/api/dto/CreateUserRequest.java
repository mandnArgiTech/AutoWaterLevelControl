package com.flm.platform.api.dto;

import com.flm.platform.common.UserRole;
import jakarta.validation.constraints.Email;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;

public record CreateUserRequest(
    @NotBlank @Email String email,
    @NotBlank String password,
    @NotBlank String displayName,
    @NotNull UserRole role
) {}
