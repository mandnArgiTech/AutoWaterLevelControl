package com.flm.platform.security;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.flm.platform.domain.PlatformUserRepository;
import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.http.MediaType;
import org.springframework.security.core.Authentication;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;
import java.io.IOException;
import java.util.Map;

/**
 * Blocks API access until the user changes their initial password.
 */
@Component
public class MustChangePasswordFilter extends OncePerRequestFilter {

    private final PlatformUserRepository userRepository;
    private final ObjectMapper objectMapper;

    public MustChangePasswordFilter(PlatformUserRepository userRepository, ObjectMapper objectMapper) {
        this.userRepository = userRepository;
        this.objectMapper = objectMapper;
    }

    @Override
    protected boolean shouldNotFilter(HttpServletRequest request) {
        String path = request.getRequestURI();
        return path.startsWith("/api/auth/login")
            || path.startsWith("/api/auth/change-password")
            || path.startsWith("/actuator/");
    }

    @Override
    protected void doFilterInternal(
        HttpServletRequest request,
        HttpServletResponse response,
        FilterChain filterChain
    ) throws ServletException, IOException {
        Authentication auth = SecurityContextHolder.getContext().getAuthentication();
        if (auth != null && auth.getPrincipal() instanceof AuthPrincipal principal) {
            boolean mustChange = userRepository.findById(principal.userId())
                .map(u -> u.isMustChangePassword())
                .orElse(false);
            if (mustChange) {
                response.setStatus(HttpServletResponse.SC_FORBIDDEN);
                response.setContentType(MediaType.APPLICATION_JSON_VALUE);
                objectMapper.writeValue(response.getWriter(), Map.of(
                    "message", "Password change required",
                    "code", "PASSWORD_CHANGE_REQUIRED"
                ));
                return;
            }
        }
        filterChain.doFilter(request, response);
    }
}
