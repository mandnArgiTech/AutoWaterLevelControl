/**
 * @file MQTTManager.cpp
 * @brief MQTT client with device-scoped topics
 * 
 * @author FluidLevelMonitor Project
 * @version 1.2.0
 */

#include "MQTTManager.h"
#include "../version.h"
#include "../utils/Log.h"

MQTTManager* MQTTManager::_instance = nullptr;

// =============================================================================
// SECTION 1: SINGLETON
// =============================================================================

MQTTManager& MQTTManager::getInstance() {
    static MQTTManager instance;
    return instance;
}

// =============================================================================
// SECTION 2: CONSTRUCTOR
// =============================================================================

MQTTManager::MQTTManager()
    : _mqttClient(_wifiClient)
    , _initialized(false)
    , _state(MQTTState::DISABLED)
    , _messageCallback(nullptr)
    , _stateCallback(nullptr)
    , _lastConnectAttempt(0)
    , _reconnectDelayMs(MQTT_RECONNECT_INTERVAL)
    , _publishCount(0)
    , _publishErrors(0) {

    _instance = this;
}

// =============================================================================
// SECTION 3: INITIALIZATION
// =============================================================================

ErrorCode MQTTManager::begin() {
    FLM_LOG_INFO("MQTT", "Initializing...");

    MQTTConfig& config = ConfigManager::getInstance().getMQTTConfig();

    if (!config.enabled) {
        FLM_LOG_INFO("MQTT", "disabled");
        setState(MQTTState::DISABLED);
        _initialized = true;
        return ErrorCode::ERR_MQTT_DISABLED;
    }

    if (config.server.length() == 0) {
        FLM_LOG_WARN("MQTT", "no broker configured");
        setState(MQTTState::DISABLED);
        return ErrorHandler::getInstance().logError(ErrorCode::ERR_MQTT_INIT, "No server");
    }

    buildDeviceTag();

    _mqttClient.setServer(config.server.c_str(), config.port);
    _mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
    _mqttClient.setKeepAlive(MQTT_KEEPALIVE);
    _mqttClient.setCallback(mqttCallback);

    FLM_LOG_INFO("MQTT", "broker %s:%u", config.server.c_str(), (unsigned)config.port);
    FLM_LOG_DEBUG("MQTT", "device %s client %s", _deviceTag.c_str(), _uniqueClientId.c_str());

    setState(MQTTState::DISCONNECTED);
    _initialized = true;

    FLM_LOG_INFO("MQTT", "ready");
    return ErrorCode::ERR_NONE;
}

// =============================================================================
// SECTION 4: DEVICE TAG
// =============================================================================

void MQTTManager::buildDeviceTag() {
    MQTTConfig& config = ConfigManager::getInstance().getMQTTConfig();
    String chipId = String(ESP.getChipId(), HEX);

    if (config.deviceName.length() > 0) {
        _deviceTag = config.deviceName + "_" + chipId;
    } else {
        _deviceTag = "flm_" + chipId;
    }

    _uniqueClientId = config.clientId + "_" + chipId;
}

// =============================================================================
// SECTION 5: CONNECTION MANAGEMENT
// =============================================================================

ErrorCode MQTTManager::connect() {
    if (_state == MQTTState::DISABLED) {
        return ErrorCode::ERR_MQTT_DISABLED;
    }

    MQTTConfig& config = ConfigManager::getInstance().getMQTTConfig();

    FLM_LOG_INFO("MQTT", "connecting %s:%u", config.server.c_str(), (unsigned)config.port);
    setState(MQTTState::CONNECTING);
    _lastConnectAttempt = millis();

    String willTopic = getTopic("status");
    const char* willMsg = "{\"online\":false}";

    bool connected = false;

    if (config.username.length() > 0) {
        connected = _mqttClient.connect(
            _uniqueClientId.c_str(),
            config.username.c_str(),
            config.password.c_str(),
            willTopic.c_str(),
            0, true, willMsg
        );
    } else {
        connected = _mqttClient.connect(
            _uniqueClientId.c_str(),
            willTopic.c_str(),
            0, true, willMsg
        );
    }

    if (connected) {
        setState(MQTTState::CONNECTED);
        _reconnectDelayMs = MQTT_RECONNECT_INTERVAL;
        FLM_LOG_INFO("MQTT", "connected");
        publishStatus();
        subscribe("command");
#if defined(FLM_ROLE_MOTOR_RELAY) || defined(FLM_ROLE_MOTOR_SMS)
        subscribe("motor/command");
#endif
        return ErrorCode::ERR_NONE;
    }

    setState(MQTTState::DISCONNECTED);
    uint32_t next = _reconnectDelayMs * 2;
    if (next < MQTT_RECONNECT_INTERVAL) next = MQTT_RECONNECT_INTERVAL;
    if (next > MQTT_RECONNECT_MAX_MS) next = MQTT_RECONNECT_MAX_MS;
    _reconnectDelayMs = next;

    int st = _mqttClient.state();
    const char* reason = "unknown";
    switch (st) {
        case -4: reason = "TIMEOUT";           break;
        case -3: reason = "CONNECTION_LOST";    break;
        case -2: reason = "CONNECT_FAILED";     break;
        case -1: reason = "DISCONNECTED";       break;
        case  1: reason = "BAD_PROTOCOL";       break;
        case  2: reason = "BAD_CLIENT_ID";      break;
        case  3: reason = "UNAVAILABLE";        break;
        case  4: reason = "BAD_CREDENTIALS";    break;
        case  5: reason = "UNAUTHORIZED";       break;
    }
    char errBuf[64];
    snprintf(errBuf, sizeof(errBuf), "State: %d (%s)", st, reason);

    return ErrorHandler::getInstance().logError(ErrorCode::ERR_MQTT_CONNECT, errBuf);
}

void MQTTManager::disconnect() {
    if (_mqttClient.connected()) {
        String statusTopic = getTopic("status");
        _mqttClient.publish(statusTopic.c_str(), "{\"online\":false}", true);
        _mqttClient.disconnect();
    }
    setState(MQTTState::DISCONNECTED);
    FLM_LOG_INFO("MQTT", "disconnected");
}

void MQTTManager::prepareForOTA() {
    if (_state == MQTTState::DISABLED) return;
    disconnect();
    _mqttClient.setBufferSize(128);
    FLM_LOG_INFO("MQTT", "prepared for OTA");
}

void MQTTManager::restoreAfterOTA() {
    if (_state == MQTTState::DISABLED) return;
    _mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
    FLM_LOG_INFO("MQTT", "restored after OTA abort");
}

bool MQTTManager::isConnected() {
    return _mqttClient.connected();
}

bool MQTTManager::isEnabled() const {
    return _state != MQTTState::DISABLED;
}

// =============================================================================
// SECTION 6: LOOP
// =============================================================================

void MQTTManager::loop() {
    if (_state == MQTTState::DISABLED) return;
    _mqttClient.loop();
    checkConnection();
}

void MQTTManager::checkConnection() {
    if (_state == MQTTState::DISABLED) return;

    if (_state == MQTTState::CONNECTED && !_mqttClient.connected()) {
        FLM_LOG_WARN("MQTT", "connection lost");
        setState(MQTTState::DISCONNECTED);
        ErrorHandler::getInstance().logError(ErrorCode::ERR_MQTT_DISCONNECTED);
    }

    if (_state == MQTTState::DISCONNECTED) {
        if ((unsigned long)(millis() - _lastConnectAttempt) >= _reconnectDelayMs) {
            connect();
        }
    }
}

// =============================================================================
// SECTION 7: PUBLISHING
// =============================================================================

bool MQTTManager::publishWaterLevel(const String& json) {
    return publish("level", json, false);
}

bool MQTTManager::publish(const String& subtopic, const String& payload, bool retained) {
    if (_state != MQTTState::CONNECTED) {
        _publishErrors++;
        return false;
    }

    String fullTopic = getTopic(subtopic);
    bool success = _mqttClient.publish(fullTopic.c_str(), payload.c_str(), retained);

    if (success) {
        _publishCount++;
        if (ConfigManager::getInstance().getSystemConfig().debugEnabled) {
            FLM_LOG_DEBUG("MQTT", "pub %s", fullTopic.c_str());
        }
    } else {
        _publishErrors++;
        ErrorHandler::getInstance().logError(ErrorCode::ERR_MQTT_PUBLISH, fullTopic);
    }

    return success;
}

bool MQTTManager::publishStatus() {
    JsonDocument doc;

    doc["online"] = true;
    doc["firmware"] = Version::getFirmware();
    doc["deviceTag"] = _deviceTag;
    doc["deviceName"] = ConfigManager::getInstance().getMQTTConfig().deviceName;
    doc["chipId"] = String(ESP.getChipId(), HEX);
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["uptime"] = millis();
    doc["freeHeap"] = ESP.getFreeHeap();

    String payload;
    serializeJson(doc, payload);

    return publish("status", payload, true);
}

// =============================================================================
// SECTION 8: SUBSCRIPTION
// =============================================================================

bool MQTTManager::subscribe(const String& subtopic) {
    if (_state != MQTTState::CONNECTED) return false;

    String fullTopic = getTopic(subtopic);
    bool success = _mqttClient.subscribe(fullTopic.c_str());

    if (success) {
        Serial.printf("[MQTTManager] Subscribed to %s\n", fullTopic.c_str());
    } else {
        ErrorHandler::getInstance().logError(ErrorCode::ERR_MQTT_SUBSCRIBE, fullTopic);
    }

    return success;
}

// =============================================================================
// SECTION 9: CALLBACKS
// =============================================================================

void MQTTManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance) {
        String topicStr = String(topic);
        String payloadStr;
        payloadStr.reserve(length);
        for (unsigned int i = 0; i < length; i++) {
            payloadStr += (char)payload[i];
        }
        _instance->handleMessage(topicStr, payloadStr);
    }
}

void MQTTManager::handleMessage(const String& topic, const String& payload) {
    Serial.printf("[MQTTManager] Message: %s = %s\n", topic.c_str(), payload.c_str());
    if (_messageCallback) {
        _messageCallback(topic, payload);
    }
}

void MQTTManager::setMessageCallback(MQTTMessageCallback callback) {
    _messageCallback = callback;
}

void MQTTManager::setStateCallback(MQTTStateCallback callback) {
    _stateCallback = callback;
}

// =============================================================================
// SECTION 10: STATE MANAGEMENT
// =============================================================================

void MQTTManager::setState(MQTTState newState) {
    if (_state != newState) {
        _state = newState;
        if (_stateCallback) {
            _stateCallback(_state);
        }
    }
}

// =============================================================================
// SECTION 11: TOPIC & STATUS
// =============================================================================

/**
 * Full topic: {deviceTag}/{topicPrefix}/{subtopic}
 * Example:    tank1_a9ad51/water/level
 */
String MQTTManager::getTopic(const String& subtopic) const {
    MQTTConfig& config = ConfigManager::getInstance().getMQTTConfig();
    return _deviceTag + "/" + config.topicPrefix + "/" + subtopic;
}

String MQTTManager::getStatusJson() {
    JsonDocument doc;
    MQTTConfig& config = ConfigManager::getInstance().getMQTTConfig();

    doc["enabled"] = config.enabled;
    doc["state"] = stateToString(_state);
    doc["connected"] = isConnected();
    doc["server"] = config.server;
    doc["port"] = config.port;
    doc["deviceTag"] = _deviceTag;
    doc["deviceName"] = config.deviceName;
    doc["topicPrefix"] = config.topicPrefix;
    doc["topicExample"] = getTopic("level");
    doc["publishInterval"] = config.publishInterval;
    doc["publishCount"] = _publishCount;
    doc["publishErrors"] = _publishErrors;

    String output;
    serializeJson(doc, output);
    return output;
}

String MQTTManager::stateToString(MQTTState state) {
    switch (state) {
        case MQTTState::DISABLED:     return "disabled";
        case MQTTState::DISCONNECTED: return "disconnected";
        case MQTTState::CONNECTING:   return "connecting";
        case MQTTState::CONNECTED:    return "connected";
        default:                      return "unknown";
    }
}
