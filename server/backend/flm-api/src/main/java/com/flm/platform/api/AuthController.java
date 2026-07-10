package com.flm.platform.api;

import com.flm.platform.api.dto.ChangePasswordRequest;
import com.flm.platform.api.dto.LoginRequest;
import com.flm.platform.api.dto.LoginResponse;
import com.flm.platform.api.service.AuthService;
import com.flm.platform.security.AuthPrincipal;
import jakarta.servlet.http.Cookie;
import jakarta.servlet.http.HttpServletResponse;
import jakarta.validation.Valid;
import org.springframework.http.ResponseEntity;
import org.springframework.security.core.annotation.AuthenticationPrincipal;
import org.springframework.web.bind.annotation.*;
import java.util.Map;

@RestController
@RequestMapping("/api/auth")
public class AuthController {

    private static final String REFRESH_COOKIE = "flm_refresh";

    private final AuthService authService;

    public AuthController(AuthService authService) {
        this.authService = authService;
    }

    @PostMapping("/login")
    public LoginResponse login(@Valid @RequestBody LoginRequest request, HttpServletResponse response) {
        AuthService.LoginResult result = authService.login(request);
        setRefreshCookie(response, result.refreshToken());
        return result.response();
    }

    @PostMapping("/refresh")
    public LoginResponse refresh(
        @CookieValue(name = REFRESH_COOKIE, required = false) String refreshToken,
        HttpServletResponse response
    ) {
        if (refreshToken == null) {
            throw new IllegalArgumentException("Refresh token missing");
        }
        AuthService.LoginResult result = authService.refresh(refreshToken);
        setRefreshCookie(response, result.refreshToken());
        return result.response();
    }

    @PostMapping("/logout")
    public ResponseEntity<Void> logout(
        @CookieValue(name = REFRESH_COOKIE, required = false) String refreshToken,
        HttpServletResponse response
    ) {
        authService.logout(refreshToken);
        clearRefreshCookie(response);
        return ResponseEntity.noContent().build();
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

    @PostMapping("/mfa/enroll")
    public Map<String, String> enrollMfa(@AuthenticationPrincipal AuthPrincipal principal) {
        String uri = authService.beginMfaEnrollment(principal.userId());
        return Map.of("provisioningUri", uri);
    }

    @PostMapping("/mfa/confirm")
    public ResponseEntity<Void> confirmMfa(
        @AuthenticationPrincipal AuthPrincipal principal,
        @RequestBody Map<String, String> body
    ) {
        authService.confirmMfaEnrollment(principal.userId(), body.get("otpCode"));
        return ResponseEntity.noContent().build();
    }

    @PostMapping("/step-up")
    public Map<String, String> stepUp(
        @AuthenticationPrincipal AuthPrincipal principal,
        @RequestBody Map<String, String> body
    ) {
        String token = authService.stepUp(principal.userId(), body.get("password"), body.get("otpCode"));
        return Map.of("stepUpToken", token);
    }

    private void setRefreshCookie(HttpServletResponse response, String token) {
        Cookie cookie = new Cookie(REFRESH_COOKIE, token);
        cookie.setHttpOnly(true);
        cookie.setSecure(true);
        cookie.setPath("/api/auth");
        cookie.setMaxAge(60 * 60 * 24 * 30);
        cookie.setAttribute("SameSite", "Strict");
        response.addCookie(cookie);
    }

    private void clearRefreshCookie(HttpServletResponse response) {
        Cookie cookie = new Cookie(REFRESH_COOKIE, "");
        cookie.setHttpOnly(true);
        cookie.setSecure(true);
        cookie.setPath("/api/auth");
        cookie.setMaxAge(0);
        response.addCookie(cookie);
    }
}
