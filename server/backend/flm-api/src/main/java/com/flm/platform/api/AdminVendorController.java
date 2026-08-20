package com.flm.platform.api;

import com.flm.platform.api.dto.CreateUserRequest;
import com.flm.platform.api.dto.UserSummary;
import com.flm.platform.api.dto.VendorSummary;
import com.flm.platform.api.service.VendorUserService;
import jakarta.validation.Valid;
import org.springframework.web.bind.annotation.*;
import java.util.List;
import java.util.Map;
import java.util.UUID;

@RestController
@RequestMapping("/api/admin/vendors")
public class AdminVendorController {

    private final VendorUserService vendorUserService;

    public AdminVendorController(VendorUserService vendorUserService) {
        this.vendorUserService = vendorUserService;
    }

    @GetMapping
    public List<VendorSummary> list() {
        return vendorUserService.listVendors();
    }

    @PostMapping
    public VendorSummary create(@RequestBody Map<String, String> body) {
        return vendorUserService.createVendor(body.get("code"), body.get("name"), body.get("vendorType"));
    }
}

@RestController
@RequestMapping("/api/vendors/{vendorId}/users")
class VendorUserController {

    private final VendorUserService vendorUserService;

    VendorUserController(VendorUserService vendorUserService) {
        this.vendorUserService = vendorUserService;
    }

    @GetMapping
    public List<UserSummary> list(@PathVariable UUID vendorId) {
        return vendorUserService.listUsers(vendorId);
    }

    @PostMapping
    public UserSummary create(@PathVariable UUID vendorId, @Valid @RequestBody CreateUserRequest req) {
        return vendorUserService.createUser(vendorId, req);
    }
}
