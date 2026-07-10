package com.flm.platform.api.service;

import com.flm.platform.api.dto.ChangePasswordRequest;
import com.flm.platform.api.dto.LoginRequest;
import com.flm.platform.api.dto.LoginResponse;
import com.flm.platform.common.UserRole;
import com.flm.platform.domain.PlatformUser;
import com.flm.platform.domain.PlatformUserRepository;
import com.flm.platform.security.AuthPrincipal;
import com.flm.platform.security.JwtService;
import org.springframework.security.authentication.BadCredentialsException;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import java.time.Instant;
import java.util.UUID;

@Service
public class AuthService {

    private final PlatformUserRepository userRepository;
    private final PasswordEncoder passwordEncoder;
    private final JwtService jwtService;

    public AuthService(
        PlatformUserRepository userRepository,
        PasswordEncoder passwordEncoder,
        JwtService jwtService
    ) {
        this.userRepository = userRepository;
        this.passwordEncoder = passwordEncoder;
        this.jwtService = jwtService;
    }

    @Transactional
    public LoginResponse login(LoginRequest req) {
        PlatformUser user = userRepository.findByEmailIgnoreCase(req.username())
            .filter(PlatformUser::isActive)
            .orElseThrow(() -> new BadCredentialsException("Invalid credentials"));

        if (!passwordEncoder.matches(req.password(), user.getPasswordHash())) {
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

        user.setLastLoginAt(Instant.now());

        return toLoginResponse(user, jwtService.createToken(toPrincipal(user)));
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

        return toLoginResponse(user, jwtService.createToken(toPrincipal(user)));
    }

    @Transactional(readOnly = true)
    public LoginResponse profile(AuthPrincipal principal) {
        PlatformUser user = userRepository.findById(principal.userId())
            .filter(PlatformUser::isActive)
            .orElseThrow(() -> new BadCredentialsException("User not found"));
        return toLoginResponse(user, null);
    }

    private AuthPrincipal toPrincipal(PlatformUser user) {
        return new AuthPrincipal(
            user.getId(),
            user.getEmail(),
            user.getRole(),
            user.getVendor() != null ? user.getVendor().getId() : null,
            user.getVendor() != null ? user.getVendor().getCode() : null
        );
    }

    private LoginResponse toLoginResponse(PlatformUser user, String token) {
        return new LoginResponse(
            token,
            user.getId(),
            user.getEmail(),
            user.getDisplayName(),
            user.getRole(),
            user.getVendor() != null ? user.getVendor().getId() : null,
            user.getVendor() != null ? user.getVendor().getCode() : null,
            user.getVendor() != null ? user.getVendor().getName() : null,
            user.isMustChangePassword()
        );
    }
}
