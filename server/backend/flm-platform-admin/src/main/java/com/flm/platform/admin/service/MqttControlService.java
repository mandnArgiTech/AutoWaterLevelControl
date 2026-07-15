package com.flm.platform.admin.service;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.flm.platform.mqtt.MqttProperties;
import jakarta.annotation.PostConstruct;
import org.eclipse.paho.client.mqttv3.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;

@Service
public class MqttControlService {

    private static final Logger log = LoggerFactory.getLogger(MqttControlService.class);
    private static final String CONTROL_TOPIC = "$CONTROL/dynamic-security/v1";
    private static final String RESPONSE_TOPIC = "$CONTROL/dynamic-security/v1/response";

    private final MqttProperties mqttProperties;
    private final String dynsecUser;
    private final String dynsecPass;
    private final long timeoutMs;
    private final ObjectMapper objectMapper;

    public MqttControlService(
        MqttProperties mqttProperties,
        @Value("${flm.mqtt.dynsec-admin-user:flmDynsecAdmin}") String dynsecUser,
        @Value("${flm.mqtt.dynsec-admin-password:flmDynsecPass}") String dynsecPass,
        @Value("${flm.mqtt.control-timeout-ms:5000}") long timeoutMs,
        ObjectMapper objectMapper
    ) {
        this.mqttProperties = mqttProperties;
        this.dynsecUser = dynsecUser;
        this.dynsecPass = dynsecPass;
        this.timeoutMs = timeoutMs;
        this.objectMapper = objectMapper;
    }

    @PostConstruct
    public void ensureBridgeSysAcls() {
        try {
            addRoleAcl("bridge", "subscribePattern", "$SYS/#", true);
            addRoleAcl("bridge", "publishClientReceive", "$SYS/#", true);
            log.info("Ensured bridge role can subscribe to $SYS/#");
        } catch (Exception e) {
            log.warn("Could not ensure bridge $SYS ACL (broker stats may be empty): {}", e.getMessage());
        }
    }

    public Map<String, Object> status() {
        Map<String, Object> result = new LinkedHashMap<>();
        result.put("brokerUrl", mqttProperties.getBrokerUrl());
        result.put("tlsEnabled", mqttProperties.isTlsEnabled());
        try {
            JsonNode clients = sendCommand(Map.of("command", "listClients", "verbose", false));
            result.put("connected", true);
            result.put("clients", clients);
        } catch (Exception e) {
            result.put("connected", false);
            result.put("error", e.getMessage());
        }
        return result;
    }

    public JsonNode listClients() {
        return sendCommand(Map.of("command", "listClients", "verbose", true));
    }

    public JsonNode listRoles() {
        return sendCommand(Map.of("command", "listRoles", "verbose", true));
    }

    public void createClient(String username, String password) {
        sendCommand(Map.of("command", "createClient", "username", username, "password", password));
    }

    public void setClientPassword(String username, String password) {
        sendCommand(Map.of("command", "setClientPassword", "username", username, "password", password));
    }

    public void deleteClient(String username) {
        if ("flmServerAdmin".equals(username) || dynsecUser.equals(username)) {
            throw new IllegalArgumentException("Cannot delete protected MQTT client");
        }
        sendCommand(Map.of("command", "deleteClient", "username", username));
    }

    public void createRole(String roleName) {
        sendCommand(Map.of("command", "createRole", "rolename", roleName));
    }

    public void addRoleAcl(String roleName, String aclType, String topic, boolean allow) {
        sendCommand(Map.of(
            "command", "addRoleACL",
            "rolename", roleName,
            "acltype", aclType,
            "topic", topic,
            "allow", allow,
            "priority", -1
        ));
    }

    public Map<String, Object> provisionDevice(String username, String password, String deviceTag) {
        String roleName = "device_" + deviceTag.replaceAll("[^a-zA-Z0-9_]", "_");
        try {
            createRole(roleName);
        } catch (Exception ignored) {
            // role may exist
        }
        addRoleAcl(roleName, "publishClientSend", deviceTag + "/water/level", true);
        addRoleAcl(roleName, "publishClientSend", deviceTag + "/water/status", true);
        addRoleAcl(roleName, "publishClientReceive", deviceTag + "/water/command", true);
        addRoleAcl(roleName, "subscribePattern", deviceTag + "/water/command", true);
        addRoleAcl(roleName, "publishClientSend", deviceTag + "/motor/status", true);
        addRoleAcl(roleName, "publishClientReceive", deviceTag + "/motor/command", true);
        addRoleAcl(roleName, "subscribePattern", deviceTag + "/motor/command", true);
        try {
            createClient(username, password);
        } catch (Exception e) {
            setClientPassword(username, password);
        }
        sendCommand(Map.of(
            "command", "addClientRole",
            "username", username,
            "rolename", roleName,
            "priority", -1
        ));
        return Map.of("username", username, "role", roleName, "deviceTag", deviceTag);
    }

    private JsonNode sendCommand(Map<String, Object> command) {
        String correlation = UUID.randomUUID().toString();
        Map<String, Object> payload = Map.of("commands", List.of(command));
        ExecutorService executor = Executors.newSingleThreadExecutor();
        try {
            CompletableFuture<JsonNode> future = new CompletableFuture<>();
            MqttClient client = newClient("flm-dynsec-" + correlation.substring(0, 8));
            client.setCallback(new MqttCallback() {
                @Override
                public void connectionLost(Throwable cause) {}

                @Override
                public void messageArrived(String topic, MqttMessage message) {
                    try {
                        JsonNode node = objectMapper.readTree(new String(message.getPayload(), StandardCharsets.UTF_8));
                        if (node.has("responses")) {
                            future.complete(node.get("responses").get(0));
                        } else {
                            future.complete(node);
                        }
                    } catch (Exception e) {
                        future.completeExceptionally(e);
                    }
                }

                @Override
                public void deliveryComplete(IMqttDeliveryToken token) {}
            });
            client.connect(connectOptions());
            client.subscribe(RESPONSE_TOPIC, 1);
            String body = objectMapper.writeValueAsString(payload);
            MqttMessage msg = new MqttMessage(body.getBytes(StandardCharsets.UTF_8));
            msg.setQos(1);
            client.publish(CONTROL_TOPIC, msg);
            JsonNode response = future.get(timeoutMs, TimeUnit.MILLISECONDS);
            client.disconnect();
            client.close();
            return response;
        } catch (Exception e) {
            log.warn("MQTT control command failed: {}", e.getMessage());
            throw new IllegalStateException("MQTT control failed: " + e.getMessage(), e);
        } finally {
            executor.shutdownNow();
        }
    }

    private MqttClient newClient(String clientId) throws MqttException {
        return new MqttClient(mqttProperties.getBrokerUrl(), clientId);
    }

    private MqttConnectOptions connectOptions() {
        MqttConnectOptions opts = new MqttConnectOptions();
        opts.setAutomaticReconnect(false);
        opts.setCleanSession(true);
        opts.setUserName(dynsecUser);
        opts.setPassword(dynsecPass.toCharArray());
        opts.setConnectionTimeout(5);
        return opts;
    }
}
