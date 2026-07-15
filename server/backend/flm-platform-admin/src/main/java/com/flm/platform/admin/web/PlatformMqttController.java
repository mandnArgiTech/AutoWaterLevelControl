package com.flm.platform.admin.web;

import com.flm.platform.admin.service.AuditService;
import com.flm.platform.admin.service.MqttControlService;
import com.flm.platform.mqtt.MqttIngestService;
import com.flm.platform.mqtt.MqttTopicActivityStore;
import com.flm.platform.security.AuthPrincipal;
import com.flm.platform.security.RequiresStepUp;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.security.core.annotation.AuthenticationPrincipal;
import org.springframework.web.bind.annotation.*;
import java.time.Duration;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

@RestController
@RequestMapping("/api/admin/platform/mqtt")
public class PlatformMqttController {

    private final MqttControlService mqttControlService;
    private final AuditService auditService;
    private final MqttTopicActivityStore activityStore;
    private final MqttIngestService mqttIngestService;

    public PlatformMqttController(
        MqttControlService mqttControlService,
        AuditService auditService,
        MqttTopicActivityStore activityStore,
        MqttIngestService mqttIngestService
    ) {
        this.mqttControlService = mqttControlService;
        this.auditService = auditService;
        this.activityStore = activityStore;
        this.mqttIngestService = mqttIngestService;
    }

    @GetMapping("/status")
    @PreAuthorize("hasAuthority('PLATFORM_MQTT_READ')")
    public Map<String, Object> status() {
        Map<String, Object> result = new LinkedHashMap<>(mqttControlService.status());
        result.put("bridgeConnected", mqttIngestService.isConnected());
        result.putAll(activityStore.brokerStats());
        return result;
    }

    @GetMapping("/overview")
    @PreAuthorize("hasAuthority('PLATFORM_MQTT_READ')")
    public Map<String, Object> overview() {
        return status();
    }

    @GetMapping("/topics")
    @PreAuthorize("hasAuthority('PLATFORM_MQTT_READ')")
    public List<MqttTopicActivityStore.TopicActivity> topics(
        @RequestParam(defaultValue = "false") boolean includeSys
    ) {
        return activityStore.listTopics(includeSys, Duration.ofSeconds(60));
    }

    @GetMapping("/topic")
    @PreAuthorize("hasAuthority('PLATFORM_MQTT_READ')")
    public MqttTopicActivityStore.TopicActivity topic(@RequestParam("name") String name) {
        return activityStore.getTopic(name)
            .orElseThrow(() -> new IllegalArgumentException("Topic not seen yet: " + name));
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
