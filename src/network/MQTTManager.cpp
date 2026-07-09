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
#include "../utils/TimeManager.h"
#include <LittleFS.h>

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
    : _secureClient(nullptr)
    , _caCerts(nullptr)
    , _tlsSession(nullptr)
    , _useTls(false)
    , _tlsNeedsTime(false)
    , _mqttClient(_wifiClient)
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

    if (config.tls) {
        if (!setupTls(config)) {
            setState(MQTTState::DISABLED);
            return ErrorHandler::getInstance().logError(ErrorCode::ERR_MQTT_INIT, "TLS setup failed");
        }
        _mqttClient.setClient(*_secureClient);
    }

    _mqttClient.setServer(config.server.c_str(), config.port);
    _mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
    _mqttClient.setKeepAlive(FLM_MQTT_KEEPALIVE_S);
    _mqttClient.setCallback(mqttCallback);

    FLM_LOG_INFO("MQTT", "broker %s:%u%s", config.server.c_str(), (unsigned)config.port,
                 config.tls ? " (TLS)" : "");
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
// SECTION 4b: TLS SETUP
// =============================================================================

/**
 * Configure BearSSL for the ESP8266's tight heap:
 * - MFLN buffers (1 KB instead of 16 KB) — the single fix that stops the
 *   OOM crash/reboot loop when TLS is enabled. Requires broker-side MFLN
 *   support (any OpenSSL 1.1.1+ based broker, e.g. Mosquitto on a VPS).
 * - TLS session cache — resumed handshakes skip the expensive asymmetric
 *   crypto: reconnects go from seconds to ~100 ms with minimal heap churn.
 * - TLS 1.2 only — drops legacy protocol code paths.
 */
bool MQTTManager::setupTls(const MQTTConfig& config) {
    _secureClient = new BearSSL::WiFiClientSecure();
    _secureClient->setBufferSizes(FLM_TLS_RX_BUF, FLM_TLS_TX_BUF);
    _secureClient->setSSLVersion(BR_TLS12, BR_TLS12);

    _tlsSession = new BearSSL::Session();
    _secureClient->setSession(_tlsSession);

    if (config.tlsMode == "ca") {
        File f = LittleFS.open(MQTT_CA_FILE, "r");
        if (!f) {
            FLM_LOG_ERROR("MQTT", "tlsMode=ca but %s not found on LittleFS", MQTT_CA_FILE);
            return false;
        }
        String pem = f.readString();
        f.close();
        _caCerts = new BearSSL::X509List(pem.c_str());
        if (_caCerts->getCount() == 0) {
            FLM_LOG_ERROR("MQTT", "no valid certificate in %s", MQTT_CA_FILE);
            return false;
        }
        _secureClient->setTrustAnchors(_caCerts);
        _tlsNeedsTime = true;  // X.509 validity check needs a real clock (NTP)
        FLM_LOG_INFO("MQTT", "TLS: CA validation, %u cert(s)", (unsigned)_caCerts->getCount());
    } else if (config.tlsMode == "fingerprint") {
        if (config.fingerprint.length() == 0
            || !_secureClient->setFingerprint(config.fingerprint.c_str())) {
            FLM_LOG_ERROR("MQTT", "invalid TLS fingerprint '%s'", config.fingerprint.c_str());
            return false;
        }
        FLM_LOG_INFO("MQTT", "TLS: fingerprint pinning");
    } else {
        _secureClient->setInsecure();
        FLM_LOG_WARN("MQTT", "TLS: encrypted but certificate NOT validated");
    }

    _useTls = true;
    return true;
}

bool MQTTManager::tlsReady() {
    // Cert validity check fails with an unset clock — wait for NTP first.
    if (_tlsNeedsTime && !TimeManager::getInstance().isSynchronized()) {
        FLM_LOG_DEBUG("MQTT", "TLS waiting for NTP sync");
        return false;
    }
    // A BearSSL handshake needs ~13 KB peak; attempting it with less heap
    // is exactly the OOM → panic → reboot loop. Defer instead of crashing.
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < FLM_TLS_MIN_FREE_HEAP) {
        FLM_LOG_WARN("MQTT", "TLS deferred: heap %u < %u", freeHeap,
                     (unsigned)FLM_TLS_MIN_FREE_HEAP);
        return false;
    }
    return true;
}

// =============================================================================
// SECTION 5: CONNECTION MANAGEMENT
// =============================================================================

ErrorCode MQTTManager::connect() {
    if (_state == MQTTState::DISABLED) {
        return ErrorCode::ERR_MQTT_DISABLED;
    }
    if (!WiFi.isConnected()) {
        setState(MQTTState::DISCONNECTED);
        return ErrorCode::ERR_MQTT_CONNECT;
    }
    if (_useTls && !tlsReady()) {
        // Re-arm the retry timer so checkConnection() doesn't call us every
        // loop iteration; keep the current backoff (this is not a failure).
        _lastConnectAttempt = millis();
        return ErrorCode::ERR_MQTT_CONNECT;
    }

    MQTTConfig& config = ConfigManager::getInstance().getMQTTConfig();

    FLM_LOG_INFO("MQTT", "connecting %s:%u%s heap=%u", config.server.c_str(),
                 (unsigned)config.port, _useTls ? " TLS" : "",
                 (unsigned)ESP.getFreeHeap());
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

    if (_useTls && _secureClient) {
        char sslErr[80];
        int sslCode = _secureClient->getLastSSLError(sslErr, sizeof(sslErr));
        if (sslCode != 0) {
            FLM_LOG_ERROR("MQTT", "TLS error %d: %s (heap %u)", sslCode, sslErr,
                          (unsigned)ESP.getFreeHeap());
        }
    }

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
        // PubSubClient::connect() blocks for DNS + TCP; never attempt it
        // without WiFi or every retry stalls the main loop on a TCP timeout.
        if (!WiFi.isConnected()) return;
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
        FLM_LOG_INFO("MQTT", "subscribed %s", fullTopic.c_str());
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
        payloadStr.concat(reinterpret_cast<const char*>(payload), length);
        _instance->handleMessage(topicStr, payloadStr);
    }
}

void MQTTManager::handleMessage(const String& topic, const String& payload) {
    FLM_LOG_DEBUG("MQTT", "msg %s = %s", topic.c_str(), payload.c_str());
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
    doc["tls"] = config.tls;
    if (config.tls) {
        doc["tlsMode"] = config.tlsMode;
    }
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
