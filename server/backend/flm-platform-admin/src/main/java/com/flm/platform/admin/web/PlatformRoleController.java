package com.flm.platform.admin.web;

import com.flm.platform.admin.service.RbacService;
import com.flm.platform.domain.Permission;
import com.flm.platform.domain.Role;
import com.flm.platform.security.AuthPrincipal;
import com.flm.platform.security.RequiresStepUp;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.security.core.annotation.AuthenticationPrincipal;
import org.springframework.web.bind.annotation.*;
import java.util.*;
import java.util.stream.Collectors;

@RestController
@RequestMapping("/api/admin/platform/roles")
@PreAuthorize("hasAuthority('ROLE_MANAGE')")
public class PlatformRoleController {

    private final RbacService rbacService;

    public PlatformRoleController(RbacService rbacService) {
        this.rbacService = rbacService;
    }

    @GetMapping
    public List<RoleDto> list() {
        return rbacService.allRoles().stream().map(RoleDto::from).toList();
    }

    @GetMapping("/permissions")
    public List<PermDto> permissions() {
        return rbacService.allPermissions().stream().map(PermDto::from).toList();
    }

    @PutMapping("/{roleId}/permissions")
    @RequiresStepUp
    public RoleDto updatePermissions(@PathVariable UUID roleId, @RequestBody Set<UUID> permissionIds) {
        return RoleDto.from(rbacService.updateRolePermissions(roleId, permissionIds));
    }

    public record PermDto(UUID id, String key, String description) {
        static PermDto from(Permission p) {
            return new PermDto(p.getId(), p.getKey(), p.getDescription());
        }
    }

    public record RoleDto(UUID id, String key, String name, boolean systemManaged, Set<String> permissions) {
        static RoleDto from(Role r) {
            return new RoleDto(
                r.getId(),
                r.getKey(),
                r.getName(),
                r.isSystemManaged(),
                r.getPermissions().stream().map(Permission::getKey).collect(Collectors.toCollection(TreeSet::new))
            );
        }
    }
}
