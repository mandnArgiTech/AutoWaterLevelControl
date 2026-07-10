package com.flm.platform.api;

import com.flm.platform.common.UserRole;
import com.flm.platform.domain.PlatformUser;
import com.flm.platform.domain.PlatformUserRepository;
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
        PlatformUserRepository userRepository,
        PasswordEncoder passwordEncoder
    ) {
        return args -> {
            if (!enabled) return;

            Vendor demo = vendorRepository.findByCode("demo").orElseGet(() -> {
                Vendor v = new Vendor();
                v.setCode("demo");
                v.setName("Demo Water Solutions");
                return vendorRepository.save(v);
            });

            seedUser(userRepository, passwordEncoder, "admin", "Platform Admin", UserRole.SUPER_ADMIN, null);
            seedUser(userRepository, passwordEncoder, "vendor", "Demo Vendor Admin", UserRole.VENDOR_ADMIN, demo);
        };
    }

    private static void seedUser(
        PlatformUserRepository userRepository,
        PasswordEncoder passwordEncoder,
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
        log.info("Seeded {}: {} / 123456 (must change password on first login)", role, username);
    }
}
