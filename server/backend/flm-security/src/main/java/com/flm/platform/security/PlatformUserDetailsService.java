package com.flm.platform.security;

import com.flm.platform.common.UserRole;
import com.flm.platform.domain.PlatformUser;
import com.flm.platform.domain.PlatformUserRepository;
import org.springframework.security.core.userdetails.User;
import org.springframework.security.core.userdetails.UserDetails;
import org.springframework.security.core.userdetails.UserDetailsService;
import org.springframework.security.core.userdetails.UsernameNotFoundException;
import org.springframework.stereotype.Service;

@Service
public class PlatformUserDetailsService implements UserDetailsService {

    private final PlatformUserRepository userRepository;

    public PlatformUserDetailsService(PlatformUserRepository userRepository) {
        this.userRepository = userRepository;
    }

    @Override
    public UserDetails loadUserByUsername(String username) throws UsernameNotFoundException {
        PlatformUser u = userRepository.findByEmailIgnoreCase(username)
            .filter(PlatformUser::isActive)
            .orElseThrow(() -> new UsernameNotFoundException("User not found"));
        return User.builder()
            .username(u.getEmail())
            .password(u.getPasswordHash())
            .roles(u.getRole().name())
            .build();
    }
}
