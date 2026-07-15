package com.flm.platform.mqtt;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.flm.platform.mqtt.telemetry.DeviceIdCache;
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
 * Subscribes to Mosquitto and enqueues typed telemetry samples for batch JDBC write.
 * Topic: {deviceTag}/{topicPrefix}/{subtopic}  e.g. tank2_34ea20/water/level
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
    private MqttClient client;

    public MqttIngestService(
        MqttProperties props,
        ObjectMapper objectMapper,
        MqttTopicActivityStore activityStore,
        DeviceIdCache deviceIdCache,
        ReadingBatchWriter batchWriter
    ) {
        this.props = props;
        this.objectMapper = objectMapper;
        this.activityStore = activityStore;
        this.deviceIdCache = deviceIdCache;
        this.batchWriter = batchWriter;
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
        String subtopic = parts[parts.length - 1];

        Optional<DeviceIdCache.CachedDevice> deviceOpt = deviceIdCache.lookup(deviceTag);
        if (deviceOpt.isEmpty()) {
            log.warn("Ignoring MQTT {} — deviceTag '{}' is not registered (POST /api/devices)",
                subtopic, deviceTag);
            return;
        }
        UUID deviceId = deviceOpt.get().deviceId();
        Instant now = Instant.now();
        JsonNode root = objectMapper.readTree(payload);

        if ("status".equals(subtopic)) {
            boolean online = root.path("online").asBoolean(false);
            Long uptimeMs = readUptimeMs(root);
            Short rssi = root.path("rssi").isNumber() ? (short) root.path("rssi").asInt() : null;
            batchWriter.enqueue(new TelemetrySample(
                deviceId, now,
                null, null, null, null, null, null, null, null,
                uptimeMs, rssi, online, false
            ));
            return;
        }

        if ("level".equals(subtopic)) {
            JsonNode level = root.path("level");
            JsonNode sensor = root.path("sensor");
            JsonNode battery = root.path("battery");

            Float pct = level.path("percentFilled").isNumber() ? (float) level.path("percentFilled").asDouble() : null;
            Float vol = level.path("volumeLiters").isNumber() ? (float) level.path("volumeLiters").asDouble() : null;
            Short waterMm = toShort(level, "waterHeightMm");
            if (waterMm == null && level.path("waterHeightCm").isNumber()) {
                waterMm = (short) Math.round(level.path("waterHeightCm").asDouble() * 10.0);
            }
            Short distMm = toShort(sensor, "distanceMm");
            if (distMm == null && sensor.path("distanceCm").isNumber()) {
                distMm = (short) Math.round(sensor.path("distanceCm").asDouble() * 10.0);
            }
            Float tempC = sensor.path("temperatureC").isNumber() ? (float) sensor.path("temperatureC").asDouble() : null;
            Short humidity = sensor.path("humidityPct").isNumber() ? (short) sensor.path("humidityPct").asInt() : null;
            Short battPct = battery.path("percent").isNumber() ? (short) battery.path("percent").asInt() : null;
            Short battMv = null;
            if (battery.path("voltage").isNumber()) {
                battMv = (short) Math.round(battery.path("voltage").asDouble() * 1000.0);
            } else if (battery.path("millivolts").isNumber()) {
                battMv = (short) battery.path("millivolts").asInt();
            }
            Long uptimeMs = readUptimeMs(root);
            Short rssi = root.path("rssi").isNumber() ? (short) root.path("rssi").asInt() : null;

            batchWriter.enqueue(new TelemetrySample(
                deviceId, now,
                pct, vol, waterMm, distMm, tempC, humidity, battMv, battPct,
                uptimeMs, rssi, true, true
            ));
            log.debug("Queued level for {} pct={}", deviceTag, pct);
        }
    }

    private static Long readUptimeMs(JsonNode root) {
        if (root.path("uptimeMs").isNumber()) return root.path("uptimeMs").asLong();
        if (root.path("uptime").isNumber()) return root.path("uptime").asLong();
        return null;
    }

    private static Short toShort(JsonNode node, String field) {
        if (node == null || !node.path(field).isNumber()) return null;
        int v = node.path(field).asInt();
        if (v < Short.MIN_VALUE || v > Short.MAX_VALUE) return null;
        return (short) v;
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
