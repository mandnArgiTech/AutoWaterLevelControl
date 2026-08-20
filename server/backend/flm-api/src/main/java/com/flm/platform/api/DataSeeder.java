package com.flm.platform.api;

import com.flm.platform.admin.service.RbacService;
import com.flm.platform.common.UserRole;
import com.flm.platform.common.VendorType;
import com.flm.platform.domain.PlatformUser;
import com.flm.platform.domain.PlatformUserRepository;
import com.flm.platform.domain.Site;
import com.flm.platform.domain.SiteRepository;
import com.flm.platform.domain.Vendor;
import com.flm.platform.domain.VendorRepository;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.CommandLineRunner;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.crypto.password.PasswordEncoder;

@Configuration
public class DataSeeder {

    private static final Logger log = LoggerFactory.getLogger(DataSeeder.class);

    @Bean
    CommandLineRunner seed(
        @Value("${flm.seed.enabled:true}") boolean enabled,
        VendorRepository vendorRepository,
        SiteRepository siteRepository,
        PlatformUserRepository userRepository,
        PasswordEncoder passwordEncoder,
        RbacService rbacService
    ) {
        return args -> {
            if (!enabled) return;

            rbacService.seedIfEmpty();

            Vendor demo = vendorRepository.findByCode("demo").orElseGet(() -> {
                Vendor v = new Vendor();
                v.setCode("demo");
                v.setName("Demo Water Solutions");
                v.setVendorType(VendorType.HOUSEHOLD);
                return vendorRepository.save(v);
            });
            if (demo.getVendorType() == null) {
                demo.setVendorType(VendorType.HOUSEHOLD);
                vendorRepository.save(demo);
            }

            if (siteRepository.findByVendorIdOrderByNameAsc(demo.getId()).isEmpty()) {
                Site home = new Site();
                home.setVendor(demo);
                home.setName("Example Home");
                home.setKind("HOME");
                home.setAddress("Demo street");
                siteRepository.save(home);
                log.info("Seeded example site for vendor demo");
            }

            seedUser(userRepository, passwordEncoder, rbacService, "admin", "Platform Admin", UserRole.SUPER_ADMIN, null);
            seedUser(userRepository, passwordEncoder, rbacService, "vendor", "Demo Vendor Admin", UserRole.VENDOR_ADMIN, demo);
        };
    }

    private static void seedUser(
        PlatformUserRepository userRepository,
        PasswordEncoder passwordEncoder,
        RbacService rbacService,
        String username,
        String displayName,
        UserRole role,
        Vendor vendor
    ) {
        if (userRepository.existsByEmailIgnoreCase(username)) {
            return;
        }
        PlatformUser user = new PlatformUser();
        user.setEmail(username);
        user.setDisplayName(displayName);
        user.setRole(role);
        user.setVendor(vendor);
        user.setPasswordHash(passwordEncoder.encode("123456"));
        user.setMustChangePassword(true);
        userRepository.save(user);
        rbacService.assignRolesToUser(user, java.util.Set.of(role.name()));
        log.info("Seeded {}: {} / 123456 (must change password on first login)", role, username);
    }
}
