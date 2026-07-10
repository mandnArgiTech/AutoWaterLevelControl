package com.flm.platform.security;

import com.flm.platform.common.UserRole;
import org.junit.jupiter.api.Test;
import java.util.List;
import java.util.UUID;
import static org.junit.jupiter.api.Assertions.*;

class JwtServiceTest {

    @Test
    void createAndParseAccessToken() {
        JwtService jwt = new JwtService("test-secret-key-at-least-32-characters-long!!", 900, 300);
        AuthPrincipal principal = new AuthPrincipal(
            UUID.randomUUID(), "admin@test", UserRole.SUPER_ADMIN, null, null,
            List.of("ROLE_MANAGE", "USER_MANAGE"), false
        );
        String token = jwt.createAccessToken(principal);
        AuthPrincipal parsed = jwt.parseAccessToken(token);
        assertEquals(principal.email(), parsed.email());
        assertEquals(2, parsed.permissions().size());
    }

    @Test
    void stepUpTokenValidates() {
        JwtService jwt = new JwtService("test-secret-key-at-least-32-characters-long!!", 900, 300);
        UUID userId = UUID.randomUUID();
        String stepUp = jwt.createStepUpToken(userId);
        assertTrue(jwt.validateStepUpToken(stepUp, userId));
        assertFalse(jwt.validateStepUpToken(stepUp, UUID.randomUUID()));
    }
}
