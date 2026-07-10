package com.flm.platform.security;

import jakarta.servlet.http.HttpServletRequest;
import org.aspectj.lang.ProceedingJoinPoint;
import org.aspectj.lang.annotation.Around;
import org.aspectj.lang.annotation.Aspect;
import org.springframework.http.HttpStatus;
import org.springframework.security.access.AccessDeniedException;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.stereotype.Component;
import org.springframework.web.context.request.RequestContextHolder;
import org.springframework.web.context.request.ServletRequestAttributes;
import org.springframework.web.server.ResponseStatusException;

@Aspect
@Component
public class StepUpAspect {

    private final JwtService jwtService;

    public StepUpAspect(JwtService jwtService) {
        this.jwtService = jwtService;
    }

    @Around("@annotation(RequiresStepUp)")
    public Object requireStepUp(ProceedingJoinPoint pjp) throws Throwable {
        var auth = SecurityContextHolder.getContext().getAuthentication();
        if (auth == null || !(auth.getPrincipal() instanceof AuthPrincipal principal)) {
            throw new AccessDeniedException("Not authenticated");
        }

        HttpServletRequest request = ((ServletRequestAttributes) RequestContextHolder.currentRequestAttributes()).getRequest();
        String stepUpHeader = request.getHeader("X-Step-Up-Token");
        if (stepUpHeader == null || !jwtService.validateStepUpToken(stepUpHeader, principal.userId())) {
            throw new ResponseStatusException(HttpStatus.FORBIDDEN, "Step-up authentication required");
        }
        return pjp.proceed();
    }
}
