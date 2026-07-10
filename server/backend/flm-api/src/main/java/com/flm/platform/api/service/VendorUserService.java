package com.flm.platform.api.service;

import com.flm.platform.api.dto.CreateUserRequest;
import com.flm.platform.api.dto.UserSummary;
import com.flm.platform.api.dto.VendorSummary;
import com.flm.platform.common.UserRole;
import com.flm.platform.domain.PlatformUser;
import com.flm.platform.domain.PlatformUserRepository;
import com.flm.platform.domain.Vendor;
import com.flm.platform.domain.VendorRepository;
import com.flm.platform.security.TenantGuard;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import java.util.List;
import java.util.UUID;

@Service
public class VendorUserService {

    private final VendorRepository vendorRepository;
    private final PlatformUserRepository userRepository;
    private final PasswordEncoder passwordEncoder;
    private final TenantGuard tenantGuard;

    public VendorUserService(
        VendorRepository vendorRepository,
        PlatformUserRepository userRepository,
        PasswordEncoder passwordEncoder,
        TenantGuard tenantGuard
    ) {
        this.vendorRepository = vendorRepository;
        this.userRepository = userRepository;
        this.passwordEncoder = passwordEncoder;
        this.tenantGuard = tenantGuard;
    }

    public List<VendorSummary> listVendors() {
        return vendorRepository.findAll().stream()
            .map(v -> new VendorSummary(v.getId(), v.getCode(), v.getName(), v.isActive(), v.getCreatedAt()))
            .toList();
    }

    @Transactional
    public VendorSummary createVendor(String code, String name) {
        if (vendorRepository.existsByCode(code)) {
            throw new IllegalArgumentException("Vendor code exists");
        }
        Vendor v = new Vendor();
        v.setCode(code.toLowerCase());
        v.setName(name);
        v = vendorRepository.save(v);
        return new VendorSummary(v.getId(), v.getCode(), v.getName(), v.isActive(), v.getCreatedAt());
    }

    public List<UserSummary> listUsers(UUID vendorId) {
        tenantGuard.requireVendorScope(vendorId);
        return userRepository.findByVendorId(vendorId).stream()
            .map(u -> new UserSummary(u.getId(), u.getEmail(), u.getDisplayName(), u.getRole(), u.isActive()))
            .toList();
    }

    @Transactional
    public UserSummary createUser(UUID vendorId, CreateUserRequest req) {
        tenantGuard.requireVendorScope(vendorId);
        if (!tenantGuard.current().canManageUsers()) {
            throw new TenantGuard.AccessDeniedException("Cannot manage users");
        }
        if (req.role() == UserRole.SUPER_ADMIN) {
            throw new IllegalArgumentException("Cannot create super admin via vendor API");
        }
        if (userRepository.existsByEmailIgnoreCase(req.email())) {
            throw new IllegalArgumentException("Email already registered");
        }
        Vendor vendor = vendorRepository.findById(vendorId)
            .orElseThrow(() -> new IllegalArgumentException("Vendor not found"));
        PlatformUser u = new PlatformUser();
        u.setVendor(vendor);
        u.setEmail(req.email().toLowerCase());
        u.setDisplayName(req.displayName());
        u.setRole(req.role());
        u.setPasswordHash(passwordEncoder.encode(req.password()));
        u.setMustChangePassword(true);
        u = userRepository.save(u);
        return new UserSummary(u.getId(), u.getEmail(), u.getDisplayName(), u.getRole(), u.isActive());
    }
}
