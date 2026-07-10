package com.flm.platform.admin.service;

import com.flm.platform.common.PermissionKey;
import com.flm.platform.common.UserRole;
import com.flm.platform.domain.*;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import java.util.*;
import java.util.stream.Collectors;

@Service
public class RbacService {

    private final PermissionRepository permissionRepository;
    private final RoleRepository roleRepository;
    private final PlatformUserRepository userRepository;

    public RbacService(
        PermissionRepository permissionRepository,
        RoleRepository roleRepository,
        PlatformUserRepository userRepository
    ) {
        this.permissionRepository = permissionRepository;
        this.roleRepository = roleRepository;
        this.userRepository = userRepository;
    }

    @Transactional(readOnly = true)
    public List<String> permissionsForUser(UUID userId) {
        PlatformUser user = userRepository.findById(userId).orElseThrow();
        if (!user.getRoles().isEmpty()) {
            return user.getRoles().stream()
                .flatMap(r -> r.getPermissions().stream())
                .map(Permission::getKey)
                .distinct()
                .sorted()
                .toList();
        }
        return defaultPermissionsForLegacyRole(user.getRole());
    }

    @Transactional(readOnly = true)
    public List<Permission> allPermissions() {
        return permissionRepository.findAll();
    }

    @Transactional(readOnly = true)
    public List<Role> allRoles() {
        return roleRepository.findAll();
    }

    @Transactional(readOnly = true)
    public Optional<Role> findRoleByKey(String key) {
        return roleRepository.findByKey(key);
    }

    @Transactional
    public Role updateRolePermissions(UUID roleId, Set<UUID> permissionIds) {
        Role role = roleRepository.findById(roleId).orElseThrow();
        if (role.isSystemManaged() && "SUPER_ADMIN".equals(role.getKey())) {
            ensureSuperAdminKeepsRoleManage(role, permissionIds);
        }
        Set<Permission> perms = new HashSet<>(permissionRepository.findAllById(permissionIds));
        role.setPermissions(perms);
        return roleRepository.save(role);
    }

    @Transactional
    public void assignRolesToUser(PlatformUser user, Set<String> roleKeys) {
        Set<Role> roles = roleKeys.stream()
            .map(k -> roleRepository.findByKey(k).orElseThrow())
            .collect(Collectors.toSet());
        user.setRoles(roles);
        userRepository.save(user);
    }

    @Transactional
    public void seedIfEmpty() {
        if (permissionRepository.count() > 0) {
            syncLegacyUsers();
            return;
        }

        Map<String, Permission> perms = new LinkedHashMap<>();
        seedPermission(perms, PermissionKey.PLATFORM_MQTT_READ, "View MQTT broker status");
        seedPermission(perms, PermissionKey.PLATFORM_MQTT_WRITE, "Manage MQTT clients and ACLs");
        seedPermission(perms, PermissionKey.PLATFORM_DB_READ, "View database health");
        seedPermission(perms, PermissionKey.PLATFORM_DB_BACKUP, "Create database backups");
        seedPermission(perms, PermissionKey.PLATFORM_DB_RESTORE, "Restore database backups");
        seedPermission(perms, PermissionKey.PLATFORM_SQL_READ, "Run read-only SQL");
        seedPermission(perms, PermissionKey.PLATFORM_SQL_WRITE, "Run write SQL with step-up");
        seedPermission(perms, PermissionKey.USER_MANAGE, "Manage users");
        seedPermission(perms, PermissionKey.ROLE_MANAGE, "Manage roles and permissions");
        seedPermission(perms, PermissionKey.VENDOR_MANAGE, "Manage vendors");
        seedPermission(perms, PermissionKey.AUDIT_READ, "View audit log");
        seedPermission(perms, PermissionKey.DEVICE_MANAGE, "Manage devices");

        createSystemRole("SUPER_ADMIN", "Platform Administrator", perms.values());
        createSystemRole("VENDOR_ADMIN", "Vendor Administrator", pick(perms,
            PermissionKey.USER_MANAGE, PermissionKey.DEVICE_MANAGE));
        createSystemRole("OPERATOR", "Operator", pick(perms, PermissionKey.DEVICE_MANAGE));
        createSystemRole("VIEWER", "Viewer", Set.of());

        syncLegacyUsers();
    }

    private void seedPermission(Map<String, Permission> perms, String key, String desc) {
        Permission p = new Permission();
        p.setKey(key);
        p.setDescription(desc);
        perms.put(key, permissionRepository.save(p));
    }

    private void createSystemRole(String key, String name, Collection<Permission> permissions) {
        Role role = new Role();
        role.setKey(key);
        role.setName(name);
        role.setSystemManaged(true);
        role.setPermissions(new HashSet<>(permissions));
        roleRepository.save(role);
    }

    private Set<Permission> pick(Map<String, Permission> perms, String... keys) {
        Set<Permission> set = new HashSet<>();
        for (String k : keys) {
            if (perms.containsKey(k)) set.add(perms.get(k));
        }
        return set;
    }

    private void ensureSuperAdminKeepsRoleManage(Role role, Set<UUID> permissionIds) {
        Permission roleManage = permissionRepository.findByKey(PermissionKey.ROLE_MANAGE).orElseThrow();
        if (!permissionIds.contains(roleManage.getId())) {
            throw new IllegalStateException("SUPER_ADMIN must retain ROLE_MANAGE permission");
        }
    }

    private void syncLegacyUsers() {
        for (PlatformUser user : userRepository.findAll()) {
            if (user.getRoles().isEmpty()) {
                roleRepository.findByKey(user.getRole().name()).ifPresent(r -> {
                    user.setRoles(Set.of(r));
                    userRepository.save(user);
                });
            }
        }
    }

    private List<String> defaultPermissionsForLegacyRole(UserRole role) {
        return switch (role) {
            case SUPER_ADMIN -> Arrays.stream(new String[]{
                PermissionKey.PLATFORM_MQTT_READ, PermissionKey.PLATFORM_MQTT_WRITE,
                PermissionKey.PLATFORM_DB_READ, PermissionKey.PLATFORM_DB_BACKUP, PermissionKey.PLATFORM_DB_RESTORE,
                PermissionKey.PLATFORM_SQL_READ, PermissionKey.PLATFORM_SQL_WRITE,
                PermissionKey.USER_MANAGE, PermissionKey.ROLE_MANAGE, PermissionKey.VENDOR_MANAGE,
                PermissionKey.AUDIT_READ, PermissionKey.DEVICE_MANAGE
            }).toList();
            case VENDOR_ADMIN -> List.of(PermissionKey.USER_MANAGE, PermissionKey.DEVICE_MANAGE);
            case OPERATOR -> List.of(PermissionKey.DEVICE_MANAGE);
            case VIEWER -> List.of();
        };
    }
}
