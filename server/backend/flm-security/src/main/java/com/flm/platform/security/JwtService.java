package com.flm.platform.security;

import io.jsonwebtoken.Claims;
import io.jsonwebtoken.Jwts;
import io.jsonwebtoken.security.Keys;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;
import javax.crypto.SecretKey;
import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.util.Collections;
import java.util.Date;
import java.util.List;
import java.util.UUID;

@Component
public class JwtService {

    private final SecretKey key;
    private final long accessExpirySeconds;
    private final long stepUpExpirySeconds;

    public JwtService(
        @Value("${flm.jwt.secret}") String secret,
        @Value("${flm.auth.access-token-ttl-seconds:900}") long accessExpirySeconds,
        @Value("${flm.auth.step-up-ttl-seconds:300}") long stepUpExpirySeconds
    ) {
        if (secret.length() < 32) {
            throw new IllegalStateException("flm.jwt.secret must be at least 32 characters");
        }
        this.key = Keys.hmacShaKeyFor(secret.getBytes(StandardCharsets.UTF_8));
        this.accessExpirySeconds = accessExpirySeconds;
        this.stepUpExpirySeconds = stepUpExpirySeconds;
    }

    public String createAccessToken(AuthPrincipal principal) {
        Instant now = Instant.now();
        var builder = Jwts.builder()
            .subject(principal.userId().toString())
            .claim("email", principal.email())
            .claim("role", principal.role().name())
            .claim("permissions", principal.permissions() != null ? principal.permissions() : List.of())
            .claim("type", "access")
            .issuedAt(Date.from(now))
            .expiration(Date.from(now.plusSeconds(accessExpirySeconds)));

        if (principal.vendorId() != null) {
            builder.claim("vendorId", principal.vendorId().toString());
        }
        if (principal.vendorCode() != null) {
            builder.claim("vendorCode", principal.vendorCode());
        }
        return builder.signWith(key).compact();
    }

    public String createStepUpToken(UUID userId) {
        Instant now = Instant.now();
        return Jwts.builder()
            .subject(userId.toString())
            .claim("type", "step_up")
            .issuedAt(Date.from(now))
            .expiration(Date.from(now.plusSeconds(stepUpExpirySeconds)))
            .signWith(key)
            .compact();
    }

    public AuthPrincipal parseAccessToken(String token) {
        Claims claims = parseClaims(token);
        if (!"access".equals(claims.get("type", String.class))) {
            throw new IllegalArgumentException("Invalid token type");
        }
        return toPrincipal(claims, false);
    }

    public boolean validateStepUpToken(String token, UUID userId) {
        try {
            Claims claims = parseClaims(token);
            if (!"step_up".equals(claims.get("type", String.class))) {
                return false;
            }
            return userId.toString().equals(claims.getSubject());
        } catch (Exception e) {
            return false;
        }
    }

    @SuppressWarnings("unchecked")
    private AuthPrincipal toPrincipal(Claims claims, boolean stepUpValid) {
        UUID vendorId = claims.get("vendorId", String.class) != null
            ? UUID.fromString(claims.get("vendorId", String.class))
            : null;
        String vendorCode = claims.get("vendorCode", String.class);
        List<String> permissions = claims.get("permissions") instanceof List<?> list
            ? (List<String>) list
            : Collections.emptyList();

        return new AuthPrincipal(
            UUID.fromString(claims.getSubject()),
            claims.get("email", String.class),
            com.flm.platform.common.UserRole.valueOf(claims.get("role", String.class)),
            vendorId,
            vendorCode,
            permissions,
            stepUpValid
        );
    }

    private Claims parseClaims(String token) {
        return Jwts.parser()
            .verifyWith(key)
            .build()
            .parseSignedClaims(token)
            .getPayload();
    }

    /** @deprecated use createAccessToken */
    public String createToken(AuthPrincipal principal) {
        return createAccessToken(principal);
    }

    /** @deprecated use parseAccessToken */
    public AuthPrincipal parse(String token) {
        return parseAccessToken(token);
    }
}
