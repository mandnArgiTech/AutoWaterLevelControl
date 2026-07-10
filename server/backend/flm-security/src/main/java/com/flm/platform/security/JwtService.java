package com.flm.platform.security;

import io.jsonwebtoken.Claims;
import io.jsonwebtoken.Jwts;
import io.jsonwebtoken.security.Keys;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;
import javax.crypto.SecretKey;
import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.util.Date;
import java.util.UUID;

@Component
public class JwtService {

    private final SecretKey key;
    private final long expirySeconds;

    public JwtService(
        @Value("${flm.jwt.secret}") String secret,
        @Value("${flm.jwt.expiry-seconds:86400}") long expirySeconds
    ) {
        this.key = Keys.hmacShaKeyFor(secret.getBytes(StandardCharsets.UTF_8));
        this.expirySeconds = expirySeconds;
    }

    public String createToken(AuthPrincipal principal) {
        Instant now = Instant.now();
        var builder = Jwts.builder()
            .subject(principal.userId().toString())
            .claim("email", principal.email())
            .claim("role", principal.role().name())
            .issuedAt(Date.from(now))
            .expiration(Date.from(now.plusSeconds(expirySeconds)));

        if (principal.vendorId() != null) {
            builder.claim("vendorId", principal.vendorId().toString());
        }
        if (principal.vendorCode() != null) {
            builder.claim("vendorCode", principal.vendorCode());
        }
        return builder.signWith(key).compact();
    }

    public AuthPrincipal parse(String token) {
        Claims claims = Jwts.parser()
            .verifyWith(key)
            .build()
            .parseSignedClaims(token)
            .getPayload();

        UUID vendorId = claims.get("vendorId", String.class) != null
            ? UUID.fromString(claims.get("vendorId", String.class))
            : null;
        String vendorCode = claims.get("vendorCode", String.class);

        return new AuthPrincipal(
            UUID.fromString(claims.getSubject()),
            claims.get("email", String.class),
            com.flm.platform.common.UserRole.valueOf(claims.get("role", String.class)),
            vendorId,
            vendorCode
        );
    }
}
