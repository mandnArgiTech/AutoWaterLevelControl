package com.flm.platform.security;

import com.eatthepath.otp.TimeBasedOneTimePasswordGenerator;
import org.springframework.stereotype.Service;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import java.security.Key;
import java.security.NoSuchAlgorithmException;
import java.time.Instant;
import java.util.Base64;

@Service
public class TotpService {

    private static final int PASSWORD_LENGTH = 6;
    private static final int TIME_STEP_SECONDS = 30;

    private final TimeBasedOneTimePasswordGenerator totp;

    public TotpService() {
        this.totp = new TimeBasedOneTimePasswordGenerator(java.time.Duration.ofSeconds(TIME_STEP_SECONDS), PASSWORD_LENGTH);
    }

    public String generateSecret() {
        try {
            KeyGenerator keyGen = KeyGenerator.getInstance("HmacSHA1");
            keyGen.init(160);
            SecretKey key = keyGen.generateKey();
            return Base64.getEncoder().encodeToString(key.getEncoded());
        } catch (NoSuchAlgorithmException e) {
            throw new IllegalStateException("Secret generation failed", e);
        }
    }

    public String getProvisioningUri(String secret, String email) {
        return "otpauth://totp/FLM:" + email + "?secret=" + secret + "&issuer=FLM&digits=6&period=30";
    }

    public boolean verify(String secretBase64, String code) {
        if (secretBase64 == null || code == null || code.isBlank()) {
            return false;
        }
        try {
            byte[] decoded = Base64.getDecoder().decode(secretBase64);
            Key key = new javax.crypto.spec.SecretKeySpec(decoded, "HmacSHA1");
            Instant now = Instant.now();
            for (int drift = -1; drift <= 1; drift++) {
                Instant step = now.plusSeconds((long) drift * TIME_STEP_SECONDS);
                String expected = String.format("%0" + PASSWORD_LENGTH + "d", totp.generateOneTimePassword(key, step));
                if (expected.equals(code.trim())) {
                    return true;
                }
            }
            return false;
        } catch (Exception e) {
            return false;
        }
    }
}
