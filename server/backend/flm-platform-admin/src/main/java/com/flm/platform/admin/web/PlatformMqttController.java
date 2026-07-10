package com.flm.platform.admin.web;

import com.flm.platform.admin.service.AuditService;
import com.flm.platform.admin.service.MqttControlService;
import com.flm.platform.security.AuthPrincipal;
import com.flm.platform.security.RequiresStepUp;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.security.core.annotation.AuthenticationPrincipal;
import org.springframework.web.bind.annotation.*;
import java.util.Map;

@RestController
@RequestMapping("/api/admin/platform/mqtt")
public class PlatformMqttController {

    private final MqttControlService mqttControlService;
    private final AuditService auditService;

    public PlatformMqttController(MqttControlService mqttControlService, AuditService auditService) {
        this.mqttControlService = mqttControlService;
        this.auditService = auditService;
    }

    @GetMapping("/status")
    @PreAuthorize("hasAuthority('PLATFORM_MQTT_READ')")
    public Map<String, Object> status() {
        return mqttControlService.status();
    }

    @GetMapping("/clients")
    @PreAuthorize("hasAuthority('PLATFORM_MQTT_READ')")
    public Object clients() {
        return mqttControlService.listClients();
    }

    @GetMapping("/roles")
    @PreAuthorize("hasAuthority('PLATFORM_MQTT_READ')")
    public Object roles() {
        return mqttControlService.listRoles();
    }

    @PostMapping("/clients")
    @PreAuthorize("hasAuthority('PLATFORM_MQTT_WRITE')")
    public void createClient(
        @AuthenticationPrincipal AuthPrincipal actor,
        @RequestBody Map<String, String> body
    ) {
        mqttControlService.createClient(body.get("username"), body.get("password"));
        auditService.log(actor, "MQTT_CLIENT_CREATE", "mqtt:" + body.get("username"), Map.of());
    }

    @DeleteMapping("/clients/{username}")
    @PreAuthorize("hasAuthority('PLATFORM_MQTT_WRITE')")
    @RequiresStepUp
    public void deleteClient(@AuthenticationPrincipal AuthPrincipal actor, @PathVariable String username) {
        mqttControlService.deleteClient(username);
        auditService.log(actor, "MQTT_CLIENT_DELETE", "mqtt:" + username, Map.of());
    }

    @PostMapping("/provision-device")
    @PreAuthorize("hasAuthority('PLATFORM_MQTT_WRITE')")
    public Map<String, Object> provisionDevice(
        @AuthenticationPrincipal AuthPrincipal actor,
        @RequestBody Map<String, String> body
    ) {
        Map<String, Object> result = mqttControlService.provisionDevice(
            body.get("username"), body.get("password"), body.get("deviceTag"));
        auditService.log(actor, "MQTT_DEVICE_PROVISION", "mqtt:" + body.get("deviceTag"), result);
        return result;
    }
}
