package com.flm.platform.api.service;

import com.flm.platform.admin.service.RbacService;
import com.flm.platform.api.dto.ChangePasswordRequest;
import com.flm.platform.api.dto.LoginRequest;
import com.flm.platform.api.dto.LoginResponse;
import com.flm.platform.common.UserRole;
import com.flm.platform.domain.PlatformUser;
import com.flm.platform.domain.PlatformUserRepository;
import com.flm.platform.security.AuthPrincipal;
import com.flm.platform.security.JwtService;
import com.flm.platform.security.RefreshTokenService;
import com.flm.platform.security.TotpService;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.security.authentication.BadCredentialsException;
import org.springframework.security.authentication.LockedException;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.List;
import java.util.Set;
import java.util.UUID;

@Service
public class AuthService {

    private final PlatformUserRepository userRepository;
    private final PasswordEncoder passwordEncoder;
    private final JwtService jwtService;
    private final RefreshTokenService refreshTokenService;
    private final TotpService totpService;
    private final RbacService rbacService;
    private final int maxFailedAttempts;
    private final int lockoutMinutes;
    private final Set<String> mfaRequiredRoles;

    public AuthService(
        PlatformUserRepository userRepository,
        PasswordEncoder passwordEncoder,
        JwtService jwtService,
        RefreshTokenService refreshTokenService,
        TotpService totpService,
        RbacService rbacService,
        @Value("${flm.auth.max-failed-attempts:5}") int maxFailedAttempts,
        @Value("${flm.auth.lockout-minutes:15}") int lockoutMinutes,
        @Value("${flm.auth.mfa-required-roles:SUPER_ADMIN}") String mfaRequiredRoles
    ) {
        this.userRepository = userRepository;
        this.passwordEncoder = passwordEncoder;
        this.jwtService = jwtService;
        this.refreshTokenService = refreshTokenService;
        this.totpService = totpService;
        this.rbacService = rbacService;
        this.maxFailedAttempts = maxFailedAttempts;
        this.lockoutMinutes = lockoutMinutes;
        this.mfaRequiredRoles = Set.of(mfaRequiredRoles.split(","));
    }

    public record LoginResult(LoginResponse response, String refreshToken) {}

    @Transactional
    public LoginResult login(LoginRequest req) {
        PlatformUser user = userRepository.findByEmailIgnoreCase(req.username())
            .filter(PlatformUser::isActive)
            .orElseThrow(() -> new BadCredentialsException("Invalid credentials"));

        if (user.getLockedUntil() != null && user.getLockedUntil().isAfter(Instant.now())) {
            throw new LockedException("Account locked. Try again later.");
        }

        if (!passwordEncoder.matches(req.password(), user.getPasswordHash())) {
            registerFailedAttempt(user);
            throw new BadCredentialsException("Invalid credentials");
        }

        if (user.getRole() != UserRole.SUPER_ADMIN) {
            if (user.getVendor() == null) {
                throw new BadCredentialsException("Vendor not assigned");
            }
            if (req.vendorCode() == null || !req.vendorCode().equalsIgnoreCase(user.getVendor().getCode())) {
                throw new BadCredentialsException("Invalid vendor portal");
            }
            if (!user.getVendor().isActive()) {
                throw new BadCredentialsException("Vendor inactive");
            }
        }

        boolean mfaRequired = mfaRequiredRoles.contains(user.getRole().name());
        if (user.isMfaEnabled()) {
            if (req.otpCode() == null || !totpService.verify(user.getOtpSecret(), req.otpCode())) {
                throw new BadCredentialsException("MFA code required or invalid");
            }
        } else if (mfaRequired && user.getOtpSecret() == null) {
            // enrollment pending — allow login but flag response
        }

        user.setFailedAttempts(0);
        user.setLockedUntil(null);
        user.setLastLoginAt(Instant.now());

        List<String> permissions = rbacService.permissionsForUser(user.getId());
        AuthPrincipal principal = new AuthPrincipal(
            user.getId(), user.getEmail(), user.getRole(),
            user.getVendor() != null ? user.getVendor().getId() : null,
            user.getVendor() != null ? user.getVendor().getCode() : null,
            permissions, false
        );
        String access = jwtService.createAccessToken(principal);
        RefreshTokenService.TokenPair refresh = refreshTokenService.create(user.getId());

        LoginResponse response = toLoginResponse(user, access, permissions, mfaRequired);
        return new LoginResult(response, refresh.rawToken());
    }

    @Transactional
    public LoginResult refresh(String rawRefreshToken) {
        UUID userId = refreshTokenService.validateAndGetUserId(rawRefreshToken);
        RefreshTokenService.TokenPair rotated = refreshTokenService.rotate(rawRefreshToken);
        PlatformUser user = userRepository.findById(userId).filter(PlatformUser::isActive).orElseThrow();
        List<String> permissions = rbacService.permissionsForUser(userId);
        AuthPrincipal principal = new AuthPrincipal(
            user.getId(), user.getEmail(), user.getRole(),
            user.getVendor() != null ? user.getVendor().getId() : null,
            user.getVendor() != null ? user.getVendor().getCode() : null,
            permissions, false
        );
        String access = jwtService.createAccessToken(principal);
        boolean mfaRequired = mfaRequiredRoles.contains(user.getRole().name());
        return new LoginResult(toLoginResponse(user, access, permissions, mfaRequired), rotated.rawToken());
    }

    @Transactional
    public void logout(String rawRefreshToken) {
        if (rawRefreshToken != null) {
            refreshTokenService.revoke(rawRefreshToken);
        }
    }

    @Transactional
    public LoginResponse changePassword(UUID userId, ChangePasswordRequest req) {
        PlatformUser user = userRepository.findById(userId)
            .filter(PlatformUser::isActive)
            .orElseThrow(() -> new BadCredentialsException("User not found"));

        if (!passwordEncoder.matches(req.currentPassword(), user.getPasswordHash())) {
            throw new BadCredentialsException("Current password is incorrect");
        }
        if (passwordEncoder.matches(req.newPassword(), user.getPasswordHash())) {
            throw new BadCredentialsException("New password must be different from the current password");
        }
        if (req.newPassword().length() < 8) {
            throw new BadCredentialsException("New password must be at least 8 characters");
        }

        user.setPasswordHash(passwordEncoder.encode(req.newPassword()));
        user.setMustChangePassword(false);
        refreshTokenService.revokeAllForUser(userId);

        List<String> permissions = rbacService.permissionsForUser(userId);
        AuthPrincipal principal = new AuthPrincipal(
            user.getId(), user.getEmail(), user.getRole(),
            user.getVendor() != null ? user.getVendor().getId() : null,
            user.getVendor() != null ? user.getVendor().getCode() : null,
            permissions, false
        );
        return toLoginResponse(user, jwtService.createAccessToken(principal), permissions,
            mfaRequiredRoles.contains(user.getRole().name()));
    }

    @Transactional(readOnly = true)
    public LoginResponse profile(AuthPrincipal principal) {
        PlatformUser user = userRepository.findById(principal.userId())
            .filter(PlatformUser::isActive)
            .orElseThrow(() -> new BadCredentialsException("User not found"));
        List<String> permissions = rbacService.permissionsForUser(user.getId());
        return toLoginResponse(user, null, permissions, mfaRequiredRoles.contains(user.getRole().name()));
    }

    @Transactional
    public String beginMfaEnrollment(UUID userId) {
        PlatformUser user = userRepository.findById(userId).orElseThrow();
        String secret = totpService.generateSecret();
        user.setOtpSecret(secret);
        user.setMfaEnabled(false);
        userRepository.save(user);
        return totpService.getProvisioningUri(secret, user.getEmail());
    }

    @Transactional
    public void confirmMfaEnrollment(UUID userId, String otpCode) {
        PlatformUser user = userRepository.findById(userId).orElseThrow();
        if (!totpService.verify(user.getOtpSecret(), otpCode)) {
            throw new BadCredentialsException("Invalid MFA code");
        }
        user.setMfaEnabled(true);
        userRepository.save(user);
    }

    public String stepUp(UUID userId, String password, String otpCode) {
        PlatformUser user = userRepository.findById(userId).orElseThrow();
        if (!passwordEncoder.matches(password, user.getPasswordHash())) {
            throw new BadCredentialsException("Invalid password");
        }
        if (user.isMfaEnabled() && (otpCode == null || !totpService.verify(user.getOtpSecret(), otpCode))) {
            throw new BadCredentialsException("MFA required for step-up");
        }
        return jwtService.createStepUpToken(userId);
    }

    private void registerFailedAttempt(PlatformUser user) {
        int attempts = user.getFailedAttempts() + 1;
        user.setFailedAttempts(attempts);
        if (attempts >= maxFailedAttempts) {
            user.setLockedUntil(Instant.now().plus(lockoutMinutes, ChronoUnit.MINUTES));
        }
        userRepository.save(user);
    }

    private LoginResponse toLoginResponse(
        PlatformUser user,
        String token,
        List<String> permissions,
        boolean mfaRequired
    ) {
        String provisioningUri = null;
        if (mfaRequired && user.getOtpSecret() != null && !user.isMfaEnabled()) {
            provisioningUri = totpService.getProvisioningUri(user.getOtpSecret(), user.getEmail());
        }
        return new LoginResponse(
            token,
            user.getId(),
            user.getEmail(),
            user.getDisplayName(),
            user.getRole(),
            user.getVendor() != null ? user.getVendor().getId() : null,
            user.getVendor() != null ? user.getVendor().getCode() : null,
            user.getVendor() != null ? user.getVendor().getName() : null,
            user.getVendor() != null ? user.getVendor().getVendorType() : null,
            user.isMustChangePassword(),
            permissions,
            mfaRequired,
            user.isMfaEnabled(),
            provisioningUri
        );
    }
}
