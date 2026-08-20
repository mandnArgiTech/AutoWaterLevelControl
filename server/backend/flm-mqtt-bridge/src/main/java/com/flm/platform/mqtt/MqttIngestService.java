package com.flm.platform.mqtt;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.flm.platform.mqtt.capability.CapabilityHandlerRegistry;
import com.flm.platform.mqtt.telemetry.DeviceHealthDao;
import com.flm.platform.mqtt.telemetry.DeviceIdCache;
import com.flm.platform.mqtt.telemetry.PendingDeviceDao;
import com.flm.platform.mqtt.telemetry.ReadingBatchWriter;
import com.flm.platform.mqtt.telemetry.TelemetrySample;
import org.eclipse.paho.client.mqttv3.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.context.properties.EnableConfigurationProperties;
import org.springframework.stereotype.Service;
import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;
import java.time.Instant;
import java.util.Optional;
import java.util.UUID;

/**
 * Subscribes to Mosquitto and routes telemetry to capability handlers.
 * Compatible topics:
 *   {tag}/water/level|status
 *   {tag}/system/announce|health
 *   {tag}/{capabilityKey}/telemetry
 */
@Service
@EnableConfigurationProperties(MqttProperties.class)
public class MqttIngestService implements MqttCallbackExtended {

    private static final Logger log = LoggerFactory.getLogger(MqttIngestService.class);

    private final MqttProperties props;
    private final ObjectMapper objectMapper;
    private final MqttTopicActivityStore activityStore;
    private final DeviceIdCache deviceIdCache;
    private final ReadingBatchWriter batchWriter;
    private final PendingDeviceDao pendingDeviceDao;
    private final DeviceHealthDao deviceHealthDao;
    private final CapabilityHandlerRegistry handlerRegistry;
    private MqttClient client;

    public MqttIngestService(
        MqttProperties props,
        ObjectMapper objectMapper,
        MqttTopicActivityStore activityStore,
        DeviceIdCache deviceIdCache,
        ReadingBatchWriter batchWriter,
        PendingDeviceDao pendingDeviceDao,
        DeviceHealthDao deviceHealthDao,
        CapabilityHandlerRegistry handlerRegistry
    ) {
        this.props = props;
        this.objectMapper = objectMapper;
        this.activityStore = activityStore;
        this.deviceIdCache = deviceIdCache;
        this.batchWriter = batchWriter;
        this.pendingDeviceDao = pendingDeviceDao;
        this.deviceHealthDao = deviceHealthDao;
        this.handlerRegistry = handlerRegistry;
    }

    @PostConstruct
    public void connect() throws MqttException {
        client = new MqttClient(props.getBrokerUrl(), props.getClientId() + "-" + System.currentTimeMillis());
        client.setCallback(this);
        MqttConnectOptions options = new MqttConnectOptions();
        options.setAutomaticReconnect(true);
        options.setCleanSession(true);
        options.setKeepAliveInterval(60);
        if (props.getUsername() != null && !props.getUsername().isBlank()) {
            options.setUserName(props.getUsername());
            options.setPassword(props.getPassword().toCharArray());
        }
        client.connect(options);
        log.info("MQTT bridge connecting to {} filter={}", props.getBrokerUrl(), props.getTopicFilter());
    }

    private void subscribeTopics() throws MqttException {
        if (client == null || !client.isConnected()) return;
        client.subscribe(props.getTopicFilter(), props.getQos());
        try {
            client.subscribe("$SYS/broker/#", 0);
            log.info("MQTT bridge subscribed filter={} and $SYS/broker/#", props.getTopicFilter());
        } catch (MqttException e) {
            log.warn("Could not subscribe $SYS/broker/# (ACL?). Live broker stats limited: {}", e.getMessage());
            log.info("MQTT bridge subscribed filter={}", props.getTopicFilter());
        }
    }

    @PreDestroy
    public void disconnect() {
        if (client != null && client.isConnected()) {
            try {
                client.disconnect();
                client.close();
            } catch (MqttException e) {
                log.warn("MQTT disconnect error: {}", e.getMessage());
            }
        }
    }

    @Override
    public void connectComplete(boolean reconnect, String serverURI) {
        try {
            subscribeTopics();
            log.info("MQTT bridge {} to {}", reconnect ? "reconnected" : "connected", serverURI);
        } catch (MqttException e) {
            log.error("MQTT subscribe failed after {}: {}", reconnect ? "reconnect" : "connect", e.getMessage());
        }
    }

    @Override
    public void connectionLost(Throwable cause) {
        log.warn("MQTT connection lost: {} (auto-reconnect enabled; will re-subscribe on connectComplete)",
            cause != null ? cause.getMessage() : "unknown");
    }

    @Override
    public void messageArrived(String topic, MqttMessage message) {
        String payload = new String(message.getPayload());
        activityStore.record(topic, payload, message.isRetained());
        if (topic.startsWith("$SYS/")) {
            return;
        }
        try {
            handleMessage(topic, payload);
        } catch (Exception e) {
            log.warn("Skip MQTT message on {}: {}", topic, e.getMessage());
        }
    }

    @Override
    public void deliveryComplete(IMqttDeliveryToken token) {}

    public void handleMessage(String topic, String payload) throws Exception {
        String[] parts = topic.split("/");
        if (parts.length < 3) return;

        String deviceTag = parts[0];
        String segment = parts[1];
        String subtopic = parts[parts.length - 1];

        Optional<DeviceIdCache.CachedDevice> deviceOpt = deviceIdCache.lookup(deviceTag);

        if ("system".equals(segment) && "announce".equals(subtopic)) {
            handleAnnounce(deviceTag, topic, payload);
            return;
        }

        if (deviceOpt.isEmpty()) {
            String chipId = null;
            String modelKey = null;
            try {
                JsonNode root = objectMapper.readTree(payload);
                chipId = text(root, "chipId", "chip_id");
                modelKey = text(root, "model_key", "modelKey");
            } catch (Exception ignored) {
            }
            pendingDeviceDao.record(deviceTag, chipId, modelKey, segment, topic, payload);
            log.debug("Queued pending deviceTag='{}' subtopic={} (not registered)", deviceTag, subtopic);
            return;
        }

        UUID deviceId = deviceOpt.get().deviceId();
        Instant now = Instant.now();
        JsonNode root = objectMapper.readTree(payload);

        if ("health".equals(subtopic) || ("system".equals(segment) && "health".equals(subtopic))) {
            upsertHealth(deviceId, now, root);
            return;
        }

        if ("status".equals(subtopic)) {
            boolean online = root.path("online").asBoolean(false);
            Long uptimeMs = readUptimeMs(root);
            Short rssi = root.path("rssi").isNumber() ? (short) root.path("rssi").asInt() : null;
            batchWriter.enqueue(new TelemetrySample(
                deviceId, now,
                null, null, null, null, null, null, null, null,
                uptimeMs, rssi, online, false
            ));
            upsertHealth(deviceId, now, root);
            return;
        }

        String capabilityKey = resolveCapability(segment, subtopic);
        if (capabilityKey != null) {
            var handler = handlerRegistry.find(capabilityKey);
            if (handler.isPresent()) {
                handler.get().handle(deviceId, deviceTag, now, root);
            } else {
                log.info("Unknown capability '{}' on topic {} — ignored", capabilityKey, topic);
            }
        }
    }

    /**
     * Map legacy and new topic shapes to a capability key.
     * water/level → measure.water_level
     * measure.env/telemetry → measure.env
     */
    static String resolveCapability(String segment, String subtopic) {
        if ("level".equals(subtopic) && ("water".equals(segment) || "measure.water_level".equals(segment))) {
            return "measure.water_level";
        }
        if ("telemetry".equals(subtopic) || "data".equals(subtopic)) {
            if (segment.contains(".")) return segment;
        }
        if (segment.startsWith("measure.") || segment.startsWith("control.") || segment.startsWith("system.")) {
            if ("telemetry".equals(subtopic) || "level".equals(subtopic) || segment.equals(subtopic)) {
                return segment;
            }
        }
        return null;
    }

    private void handleAnnounce(String deviceTag, String topic, String payload) throws Exception {
        JsonNode root = objectMapper.readTree(payload);
        String chipId = text(root, "chip_id", "chipId");
        String modelKey = text(root, "model_key", "modelKey");
        pendingDeviceDao.record(deviceTag, chipId, modelKey, "system", topic, payload);
        log.info("Announce for deviceTag='{}' model={} chip={}", deviceTag, modelKey, chipId);
    }

    private void upsertHealth(UUID deviceId, Instant now, JsonNode root) {
        Integer freeHeap = intOrNull(root, "freeHeap", "free_heap");
        Integer minFree = intOrNull(root, "minFreeHeap", "min_free_heap");
        Integer maxBlock = intOrNull(root, "maxFreeBlock", "max_free_block");
        Short cpu = root.path("cpuPercent").isNumber() ? (short) root.path("cpuPercent").asInt()
            : (root.path("cpu_percent").isNumber() ? (short) root.path("cpu_percent").asInt() : null);
        Short rssi = root.path("rssi").isNumber() ? (short) root.path("rssi").asInt() : null;
        String ip = text(root, "ip", "ip_address");
        String mac = text(root, "mac", "mac_address");
        Long uptimeMs = readUptimeMs(root);
        String firmware = text(root, "firmware");
        deviceHealthDao.upsert(deviceId, now, freeHeap, minFree, maxBlock, cpu, rssi, ip, mac, uptimeMs, firmware);
    }

    private static String text(JsonNode root, String... fields) {
        for (String f : fields) {
            if (root.path(f).isTextual()) return root.path(f).asText();
        }
        return null;
    }

    private static Integer intOrNull(JsonNode root, String... fields) {
        for (String f : fields) {
            if (root.path(f).isNumber()) return root.path(f).asInt();
        }
        return null;
    }

    private static Long readUptimeMs(JsonNode root) {
        if (root.path("uptimeMs").isNumber()) return root.path("uptimeMs").asLong();
        if (root.path("uptime").isNumber()) return root.path("uptime").asLong();
        return null;
    }

    public boolean isConnected() {
        return client != null && client.isConnected();
    }

    public void publishCommand(String deviceTag, String topicPrefix, String subtopic, String json) throws MqttException {
        if (!isConnected()) {
            throw new IllegalStateException("MQTT client not connected");
        }
        String topic = deviceTag + "/" + topicPrefix + "/" + subtopic;
        client.publish(topic, json.getBytes(), props.getQos(), false);
        activityStore.record(topic, json, false);
    }
}
