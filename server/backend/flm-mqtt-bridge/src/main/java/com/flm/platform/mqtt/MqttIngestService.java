package com.flm.platform.mqtt;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.flm.platform.domain.Device;
import com.flm.platform.domain.DeviceRepository;
import com.flm.platform.domain.LevelReading;
import com.flm.platform.domain.LevelReadingRepository;
import org.eclipse.paho.client.mqttv3.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.context.properties.EnableConfigurationProperties;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;
import java.time.Instant;
import java.util.Optional;

/**
 * Subscribes to Mosquitto and ingests ESP8266 payloads.
 * Topic: {deviceTag}/{topicPrefix}/{subtopic}  e.g. tank1_a9ad51/water/level
 *
 * Device must be registered in {@code devices} — unregistered tags are ignored.
 */
@Service
@EnableConfigurationProperties(MqttProperties.class)
public class MqttIngestService implements MqttCallbackExtended {

    private static final Logger log = LoggerFactory.getLogger(MqttIngestService.class);

    private final MqttProperties props;
    private final DeviceRepository deviceRepository;
    private final LevelReadingRepository readingRepository;
    private final ObjectMapper objectMapper;
    private MqttClient client;

    public MqttIngestService(
        MqttProperties props,
        DeviceRepository deviceRepository,
        LevelReadingRepository readingRepository,
        ObjectMapper objectMapper
    ) {
        this.props = props;
        this.deviceRepository = deviceRepository;
        this.readingRepository = readingRepository;
        this.objectMapper = objectMapper;
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
        // Initial subscribe also happens in connectComplete(false, ...)
        log.info("MQTT bridge connecting to {} filter={}", props.getBrokerUrl(), props.getTopicFilter());
    }

    private void subscribeTopics() throws MqttException {
        if (client == null || !client.isConnected()) return;
        client.subscribe(props.getTopicFilter(), props.getQos());
        log.info("MQTT bridge subscribed filter={}", props.getTopicFilter());
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
        try {
            handleMessage(topic, new String(message.getPayload()));
        } catch (Exception e) {
            log.warn("Skip MQTT message on {}: {}", topic, e.getMessage());
        }
    }

    @Override
    public void deliveryComplete(IMqttDeliveryToken token) {}

    @Transactional
    public void handleMessage(String topic, String payload) throws Exception {
        String[] parts = topic.split("/");
        if (parts.length < 3) return;

        String deviceTag = parts[0];
        String subtopic = parts[parts.length - 1];

        Optional<Device> deviceOpt = deviceRepository.findByDeviceTag(deviceTag);
        if (deviceOpt.isEmpty()) {
            // Visible in production logs — silent drop is the #1 "MQTT works but UI empty" cause
            log.warn("Ignoring MQTT {} — deviceTag '{}' is not registered (POST /api/devices)",
                subtopic, deviceTag);
            return;
        }
        Device device = deviceOpt.get();
        device.setLastSeenAt(Instant.now());

        if ("status".equals(subtopic)) {
            JsonNode root = objectMapper.readTree(payload);
            device.setOnline(root.path("online").asBoolean(false));
            deviceRepository.save(device);
            return;
        }

        if ("level".equals(subtopic)) {
            LevelReading reading = new LevelReading();
            reading.setDevice(device);
            reading.setPayloadJson(payload);
            JsonNode root = objectMapper.readTree(payload);
            JsonNode level = root.path("level");
            if (!level.isMissingNode()) {
                reading.setPercentFilled(level.path("percentFilled").asDouble());
                reading.setVolumeLiters(level.path("volumeLiters").asDouble());
            }
            JsonNode sensor = root.path("sensor");
            if (sensor.path("temperatureC").isNumber()) {
                reading.setTemperatureC(sensor.path("temperatureC").asDouble());
            }
            device.setOnline(true);
            deviceRepository.save(device);
            readingRepository.save(reading);
            log.debug("Ingested level for {} pct={}", deviceTag, reading.getPercentFilled());
        }
    }

    public void publishCommand(String deviceTag, String topicPrefix, String subtopic, String json) throws MqttException {
        if (client == null || !client.isConnected()) {
            throw new IllegalStateException("MQTT client not connected");
        }
        String topic = deviceTag + "/" + topicPrefix + "/" + subtopic;
        client.publish(topic, json.getBytes(), props.getQos(), false);
    }
}
