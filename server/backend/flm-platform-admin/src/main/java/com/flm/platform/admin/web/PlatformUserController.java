package com.flm.platform.admin.web;

import com.flm.platform.admin.service.AuditService;
import com.flm.platform.admin.service.RbacService;
import com.flm.platform.common.UserRole;
import com.flm.platform.domain.PlatformUser;
import com.flm.platform.domain.PlatformUserRepository;
import com.flm.platform.domain.Vendor;
import com.flm.platform.domain.VendorRepository;
import com.flm.platform.security.AuthPrincipal;
import com.flm.platform.security.RequiresStepUp;
import jakarta.validation.constraints.NotBlank;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.security.core.annotation.AuthenticationPrincipal;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.web.bind.annotation.*;
import java.util.*;

@RestController
@RequestMapping("/api/admin/platform/users")
@PreAuthorize("hasAuthority('USER_MANAGE')")
public class PlatformUserController {

    private final PlatformUserRepository userRepository;
    private final VendorRepository vendorRepository;
    private final PasswordEncoder passwordEncoder;
    private final RbacService rbacService;
    private final AuditService auditService;

    public PlatformUserController(
        PlatformUserRepository userRepository,
        VendorRepository vendorRepository,
        PasswordEncoder passwordEncoder,
        RbacService rbacService,
        AuditService auditService
    ) {
        this.userRepository = userRepository;
        this.vendorRepository = vendorRepository;
        this.passwordEncoder = passwordEncoder;
        this.rbacService = rbacService;
        this.auditService = auditService;
    }

    @GetMapping
    public List<PlatformUserSummary> list() {
        return userRepository.findAll().stream().map(this::toSummary).toList();
    }

    @PostMapping
    @Transactional
    public PlatformUserSummary create(@AuthenticationPrincipal AuthPrincipal actor, @RequestBody CreatePlatformUserRequest req) {
        if (userRepository.existsByEmailIgnoreCase(req.email())) {
            throw new IllegalArgumentException("Email already exists");
        }
        PlatformUser user = new PlatformUser();
        user.setEmail(req.email());
        user.setDisplayName(req.displayName());
        user.setRole(req.role());
        user.setPasswordHash(passwordEncoder.encode(req.password()));
        user.setMustChangePassword(true);
        if (req.vendorId() != null) {
            Vendor vendor = vendorRepository.findById(req.vendorId()).orElseThrow();
            user.setVendor(vendor);
        }
        rbacService.assignRolesToUser(user, Set.of(req.role().name()));
        userRepository.save(user);
        auditService.log(actor, "USER_CREATE", "user:" + user.getId(), Map.of("email", user.getEmail()));
        return toSummary(user);
    }

    @PatchMapping("/{userId}")
    @Transactional
    @RequiresStepUp
    public PlatformUserSummary update(
        @AuthenticationPrincipal AuthPrincipal actor,
        @PathVariable UUID userId,
        @RequestBody UpdatePlatformUserRequest req
    ) {
        PlatformUser user = userRepository.findById(userId).orElseThrow();
        if (req.active() != null) user.setActive(req.active());
        if (req.role() != null) {
            user.setRole(req.role());
            rbacService.assignRolesToUser(user, Set.of(req.role().name()));
        }
        if (Boolean.TRUE.equals(req.resetPassword())) {
            user.setMustChangePassword(true);
        }
        userRepository.save(user);
        auditService.log(actor, "USER_UPDATE", "user:" + userId, Map.of());
        return toSummary(user);
    }

    public record CreatePlatformUserRequest(
        @NotBlank String email,
        @NotBlank String displayName,
        @NotBlank String password,
        UserRole role,
        UUID vendorId
    ) {}

    public record UpdatePlatformUserRequest(UserRole role, Boolean active, Boolean resetPassword) {}

    private PlatformUserSummary toSummary(PlatformUser u) {
        return new PlatformUserSummary(u.getId(), u.getEmail(), u.getDisplayName(), u.getRole(), u.isActive(), u.isMustChangePassword());
    }
}
