package com.flm.platform.api;

import com.flm.platform.api.dto.ChangePasswordRequest;
import com.flm.platform.api.dto.LoginRequest;
import com.flm.platform.api.dto.LoginResponse;
import com.flm.platform.api.service.AuthService;
import com.flm.platform.security.AuthPrincipal;
import jakarta.validation.Valid;
import org.springframework.security.core.annotation.AuthenticationPrincipal;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/auth")
public class AuthController {

    private final AuthService authService;

    public AuthController(AuthService authService) {
        this.authService = authService;
    }

    @PostMapping("/login")
    public LoginResponse login(@Valid @RequestBody LoginRequest request) {
        return authService.login(request);
    }

    @PostMapping("/change-password")
    public LoginResponse changePassword(
        @AuthenticationPrincipal AuthPrincipal principal,
        @Valid @RequestBody ChangePasswordRequest request
    ) {
        return authService.changePassword(principal.userId(), request);
    }

    @GetMapping("/me")
    public LoginResponse me(@AuthenticationPrincipal AuthPrincipal principal) {
        return authService.profile(principal);
    }
}
