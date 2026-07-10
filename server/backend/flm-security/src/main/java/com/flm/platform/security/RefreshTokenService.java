package com.flm.platform.security;

import com.flm.platform.domain.RefreshToken;
import com.flm.platform.domain.RefreshTokenRepository;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.time.Instant;
import java.util.Base64;
import java.util.UUID;

@Service
public class RefreshTokenService {

    private final RefreshTokenRepository repository;
    private final long refreshTtlSeconds;
    private final SecureRandom secureRandom = new SecureRandom();

    public RefreshTokenService(
        RefreshTokenRepository repository,
        @Value("${flm.auth.refresh-token-ttl-seconds:2592000}") long refreshTtlSeconds
    ) {
        this.repository = repository;
        this.refreshTtlSeconds = refreshTtlSeconds;
    }

    public record TokenPair(String rawToken, UUID familyId) {}

    @Transactional
    public TokenPair create(UUID userId) {
        UUID familyId = UUID.randomUUID();
        return createInFamily(userId, familyId);
    }

    @Transactional
    public TokenPair rotate(String rawToken) {
        String hash = hash(rawToken);
        RefreshToken existing = repository.findByTokenHash(hash)
            .orElseThrow(() -> new IllegalArgumentException("Invalid refresh token"));

        if (existing.isRevoked() || existing.getExpiresAt().isBefore(Instant.now())) {
            repository.revokeFamily(existing.getFamilyId());
            throw new IllegalArgumentException("Refresh token reuse detected");
        }

        existing.setRevoked(true);
        repository.save(existing);
        return createInFamily(existing.getUserId(), existing.getFamilyId());
    }

    @Transactional
    public UUID validateAndGetUserId(String rawToken) {
        String hash = hash(rawToken);
        RefreshToken token = repository.findByTokenHash(hash)
            .orElseThrow(() -> new IllegalArgumentException("Invalid refresh token"));
        if (token.isRevoked() || token.getExpiresAt().isBefore(Instant.now())) {
            throw new IllegalArgumentException("Refresh token expired");
        }
        return token.getUserId();
    }

    @Transactional
    public void revoke(String rawToken) {
        repository.findByTokenHash(hash(rawToken)).ifPresent(t -> {
            t.setRevoked(true);
            repository.save(t);
        });
    }

    @Transactional
    public void revokeAllForUser(UUID userId) {
        repository.revokeAllForUser(userId);
    }

    private TokenPair createInFamily(UUID userId, UUID familyId) {
        String raw = generateRaw();
        RefreshToken entity = new RefreshToken();
        entity.setUserId(userId);
        entity.setTokenHash(hash(raw));
        entity.setFamilyId(familyId);
        entity.setExpiresAt(Instant.now().plusSeconds(refreshTtlSeconds));
        entity.setRevoked(false);
        repository.save(entity);
        return new TokenPair(raw, familyId);
    }

    private String generateRaw() {
        byte[] bytes = new byte[48];
        secureRandom.nextBytes(bytes);
        return Base64.getUrlEncoder().withoutPadding().encodeToString(bytes);
    }

    static String hash(String raw) {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            byte[] hashed = digest.digest(raw.getBytes(StandardCharsets.UTF_8));
            return Base64.getEncoder().encodeToString(hashed);
        } catch (Exception e) {
            throw new IllegalStateException(e);
        }
    }
}
