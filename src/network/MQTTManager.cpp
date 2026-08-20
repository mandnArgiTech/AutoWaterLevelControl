/**
 * @file MQTTManager.cpp
 * @brief MQTT client with device-scoped topics
 * 
 * @author FluidLevelMonitor Project
 * @version 1.2.0
 */

#include "MQTTManager.h"
#include "../version.h"
#include "../utils/HeapMonitor.h"
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
    , _publishErrors(0)
    , _lastPublishOk(false)
    , _lastPublishMs(0)
    , _logHead(0)
    , _logCount(0) {

    _lastPublishTopic[0] = '\0';
    _lastPublishPayload[0] = '\0';
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

    // Do NOT allocate BearSSL here — WiFiClientSecure + certs eat ~8–15 KB and
    // leave the heap too fragmented for the later handshake once the web UI starts.
    // connect() creates the TLS client just-in-time.
    if (config.tls) {
        if (config.tlsMode == "ca") {
            if (!LittleFS.exists(MQTT_CA_FILE)) {
                FLM_LOG_ERROR("MQTT", "tlsMode=ca but %s missing on LittleFS", MQTT_CA_FILE);
                setState(MQTTState::DISABLED);
                return ErrorHandler::getInstance().logError(ErrorCode::ERR_MQTT_INIT, "TLS CA missing");
            }
        } else if (config.tlsMode == "fingerprint" && config.fingerprint.length() == 0) {
            FLM_LOG_ERROR("MQTT", "tlsMode=fingerprint but fingerprint empty");
            setState(MQTTState::DISABLED);
            return ErrorHandler::getInstance().logError(ErrorCode::ERR_MQTT_INIT, "TLS fingerprint missing");
        }
        _useTls = true;
        FLM_LOG_INFO("MQTT", "TLS deferred alloc until connect (mode=%s)", config.tlsMode.c_str());
    } else {
        _mqttClient.setClient(_wifiClient);
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
void MQTTManager::teardownTlsClient() {
    if (_secureClient) {
        _secureClient->stop();
        delete _secureClient;
        _secureClient = nullptr;
    }
    if (_caCerts) {
        delete _caCerts;
        _caCerts = nullptr;
    }
    // Keep _tlsSession for resume across reconnect rebuilds
    _useTls = false;
    _tlsNeedsTime = false;
}

bool MQTTManager::setupTls(const MQTTConfig& config) {
    teardownTlsClient();

    _secureClient = new BearSSL::WiFiClientSecure();
    if (!_secureClient) {
        FLM_LOG_ERROR("MQTT", "TLS client alloc failed (heap %u)", (unsigned)ESP.getFreeHeap());
        return false;
    }
    // 1 KB buffers require broker MFLN (OpenSSL/Mosquitto). Without MFLN the
    // handshake fails with CONNECT_FAILED; we still use them because default
    // 16 KB RX buffers will OOM this firmware.
    _secureClient->setBufferSizes(FLM_TLS_RX_BUF, FLM_TLS_TX_BUF);
    _secureClient->setSSLVersion(BR_TLS12, BR_TLS12);

    if (!_tlsSession) {
        _tlsSession = new BearSSL::Session();
    }
    if (_tlsSession) {
        _secureClient->setSession(_tlsSession);
    }

    _tlsNeedsTime = false;
    if (config.tlsMode == "ca") {
        File f = LittleFS.open(MQTT_CA_FILE, "r");
        if (!f) {
            FLM_LOG_ERROR("MQTT", "tlsMode=ca but %s not found on LittleFS", MQTT_CA_FILE);
            teardownTlsClient();
            return false;
        }
        String pem = f.readString();
        f.close();
        _caCerts = new BearSSL::X509List(pem.c_str());
        if (!_caCerts || _caCerts->getCount() == 0) {
            FLM_LOG_ERROR("MQTT", "no valid certificate in %s", MQTT_CA_FILE);
            teardownTlsClient();
            return false;
        }
        _secureClient->setTrustAnchors(_caCerts);
        _tlsNeedsTime = true;  // X.509 validity check needs a real clock (NTP)
        FLM_LOG_INFO("MQTT", "TLS: CA validation, %u cert(s)", (unsigned)_caCerts->getCount());
    } else if (config.tlsMode == "fingerprint") {
        if (config.fingerprint.length() == 0
            || !_secureClient->setFingerprint(config.fingerprint.c_str())) {
            FLM_LOG_ERROR("MQTT", "invalid TLS fingerprint '%s'", config.fingerprint.c_str());
            teardownTlsClient();
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
    uint32_t maxBlock = ESP.getMaxFreeBlockSize();
    if (freeHeap < FLM_TLS_MIN_FREE_HEAP || maxBlock < FLM_TLS_MIN_MAX_BLOCK) {
        FLM_LOG_WARN("MQTT", "TLS deferred: heap %u maxBlock %u (need ≥%u / ≥%u)",
                     freeHeap, maxBlock,
                     (unsigned)FLM_TLS_MIN_FREE_HEAP, (unsigned)FLM_TLS_MIN_MAX_BLOCK);
        return false;
    }
    return true;
}

void MQTTManager::logTlsFailure() {
    uint32_t heap = ESP.getFreeHeap();
    uint32_t maxBlock = ESP.getMaxFreeBlockSize();
    FLM_LOG_ERROR("MQTT", "TLS fail heap=%u maxBlock=%u frag=%u%%",
                  heap, maxBlock, (unsigned)ESP.getHeapFragmentation());
    if (_secureClient) {
        char sslErr[96];
        int sslCode = _secureClient->getLastSSLError(sslErr, sizeof(sslErr));
        FLM_LOG_ERROR("MQTT", "BearSSL %d: %s MFLN=%d", sslCode, sslErr,
                      (int)_secureClient->getMFLNStatus());
    }
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

    MQTTConfig& config = ConfigManager::getInstance().getMQTTConfig();

    if (config.tls) {
        // CA mode needs NTP; fingerprint/insecure do not
        if (config.tlsMode == "ca" && !TimeManager::getInstance().isSynchronized()) {
            FLM_LOG_DEBUG("MQTT", "TLS waiting for NTP sync");
            _lastConnectAttempt = millis();
            return ErrorCode::ERR_MQTT_CONNECT;
        }
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t maxBlock = ESP.getMaxFreeBlockSize();
        if (freeHeap < FLM_TLS_MIN_FREE_HEAP || maxBlock < FLM_TLS_MIN_MAX_BLOCK) {
            FLM_LOG_WARN("MQTT", "TLS deferred: heap %u maxBlock %u (need ≥%u / ≥%u)",
                         freeHeap, maxBlock,
                         (unsigned)FLM_TLS_MIN_FREE_HEAP, (unsigned)FLM_TLS_MIN_MAX_BLOCK);
            _lastConnectAttempt = millis();
            return ErrorCode::ERR_MQTT_CONNECT;
        }
        // Rebuild client each attempt — failed handshakes fragment heap
        if (!setupTls(config)) {
            setState(MQTTState::DISCONNECTED);
            return ErrorCode::ERR_MQTT_CONNECT;
        }
        _mqttClient.setClient(*_secureClient);
        _mqttClient.setServer(config.server.c_str(), config.port);
    }

    FLM_LOG_INFO("MQTT", "connecting %s:%u%s heap=%u maxBlock=%u", config.server.c_str(),
                 (unsigned)config.port, config.tls ? " TLS" : "",
                 (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxFreeBlockSize());
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
        FLM_LOG_INFO("MQTT", "connected%s",
                     (config.tls && _secureClient && _secureClient->getMFLNStatus())
                         ? " (MFLN ok)" : "");
        publishAnnounce();
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

    if (config.tls) {
        logTlsFailure();
        teardownTlsClient();  // reclaim RAM so deferred retries can succeed
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
    String fullTopic = getTopic(subtopic);

    if (_state != MQTTState::CONNECTED) {
        _publishErrors++;
        _lastPublishOk = false;
        _lastPublishMs = millis();
        copyTrunc(_lastPublishTopic, sizeof(_lastPublishTopic), fullTopic);
        copyTrunc(_lastPublishPayload, sizeof(_lastPublishPayload), payload);
        appendLog('T', false, fullTopic, String(F("(not connected) ") ) + payload);
        return false;
    }

    // PubSubClient needs topic + payload + MQTT header inside MQTT_BUFFER_SIZE.
    const size_t need = fullTopic.length() + payload.length() + 8;
    if (need > MQTT_BUFFER_SIZE) {
        _publishErrors++;
        _lastPublishOk = false;
        _lastPublishMs = millis();
        copyTrunc(_lastPublishTopic, sizeof(_lastPublishTopic), fullTopic);
        copyTrunc(_lastPublishPayload, sizeof(_lastPublishPayload), payload);
        appendLog('T', false, fullTopic, String(F("(too large) ")) + payload);
        FLM_LOG_WARN("MQTT", "payload too large topic=%u payload=%u need=%u buf=%u",
                     (unsigned)fullTopic.length(), (unsigned)payload.length(),
                     (unsigned)need, (unsigned)MQTT_BUFFER_SIZE);
        ErrorHandler::getInstance().logError(ErrorCode::ERR_MQTT_PUBLISH, fullTopic);
        return false;
    }

    bool success = _mqttClient.publish(fullTopic.c_str(), payload.c_str(), retained);

    _lastPublishOk = success;
    _lastPublishMs = millis();
    copyTrunc(_lastPublishTopic, sizeof(_lastPublishTopic), fullTopic);
    copyTrunc(_lastPublishPayload, sizeof(_lastPublishPayload), payload);
    appendLog('T', success, fullTopic, payload);

    if (success) {
        _publishCount++;
        if (ConfigManager::getInstance().getSystemConfig().debugEnabled) {
            FLM_LOG_DEBUG("MQTT", "pub %s (%u B)", fullTopic.c_str(), (unsigned)payload.length());
        }
    } else {
        _publishErrors++;
        ErrorHandler::getInstance().logError(ErrorCode::ERR_MQTT_PUBLISH, fullTopic);
    }

    return success;
}

bool MQTTManager::publishStatus() {
    JsonDocument doc;
    HeapMonitor::sample();

    doc["online"] = true;
    doc["firmware"] = Version::getFirmware();
    doc["deviceTag"] = _deviceTag;
    doc["deviceName"] = ConfigManager::getInstance().getMQTTConfig().deviceName;
    doc["chipId"] = String(ESP.getChipId(), HEX);
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["uptime"] = millis();
    doc["uptimeMs"] = millis();
    doc["freeHeap"] = HeapMonitor::freeHeap();
    doc["minFreeHeap"] = HeapMonitor::minFreeHeap();
    doc["maxFreeBlock"] = HeapMonitor::maxFreeBlock();

    String payload;
    serializeJson(doc, payload);

    return publish("status", payload, true);
}

bool MQTTManager::publishAnnounce() {
    JsonDocument doc;
    doc["model_key"] =
#if defined(FLM_ROLE_MOTOR_RELAY)
        "motor_relay";
#elif defined(FLM_ROLE_MOTOR_SMS)
        "motor_sms";
#else
        "sensor";
#endif
    doc["chip_id"] = String(ESP.getChipId(), HEX);
    doc["firmware"] = Version::getFirmware();
    doc["comm"] = "wifi";
    JsonArray caps = doc["capabilities"].to<JsonArray>();
#if defined(FLM_ROLE_MOTOR_RELAY) || defined(FLM_ROLE_MOTOR_SMS)
    caps.add("control.motor");
#else
    caps.add("measure.water_level");
    caps.add("measure.env");
#endif
    caps.add("system.health");

    String payload;
    serializeJson(doc, payload);

    // Birth topic is outside the water prefix: {tag}/system/announce
    String topic = _deviceTag + "/system/announce";
    if (_state != MQTTState::CONNECTED) return false;
    bool ok = _mqttClient.publish(topic.c_str(), payload.c_str(), true);
    appendLog('T', ok, topic, payload);
    if (ok) {
        _publishCount++;
        FLM_LOG_INFO("MQTT", "announce published");
    } else {
        _publishErrors++;
    }
    return ok;
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
    appendLog('R', true, topic, payload);
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
    fillStatus(doc.to<JsonObject>());
    String output;
    serializeJson(doc, output);
    return output;
}

void MQTTManager::fillStatus(JsonObject doc) {
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
    doc["logCount"] = _logCount;
    doc["logCapacity"] = MQTT_LOG_CAPACITY;

    JsonObject last = doc["lastPublish"].to<JsonObject>();
    last["ok"] = _lastPublishOk;
    last["ageMs"] = (_lastPublishMs > 0) ? (int32_t)(millis() - _lastPublishMs) : (int32_t)-1;
    last["topic"] = _lastPublishTopic;
    last["payload"] = _lastPublishPayload;
}

void MQTTManager::copyTrunc(char* dst, size_t dstLen, const String& src) {
    if (!dst || dstLen == 0) return;
    size_t n = src.length();
    if (n >= dstLen) n = dstLen - 1;
    memcpy(dst, src.c_str(), n);
    dst[n] = '\0';
}

void MQTTManager::appendLog(char dir, bool ok, const String& topic, const String& payload) {
    MqttLogEntry& e = _log[_logHead];
    e.ms = millis();
    e.dir = dir;
    e.ok = ok;
    copyTrunc(e.topic, sizeof(e.topic), topic);
    copyTrunc(e.payload, sizeof(e.payload), payload);
    _logHead = (_logHead + 1) % MQTT_LOG_CAPACITY;
    if (_logCount < MQTT_LOG_CAPACITY) {
        _logCount++;
    }
}

static void mqttJsonEscapeAppend(String& out, const char* s) {
    if (!s) return;
    for (const char* p = s; *p; ++p) {
        char c = *p;
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (c == '\n') {
            out += F("\\n");
        } else if (c == '\r') {
            out += F("\\r");
        } else if ((uint8_t)c < 0x20) {
            // skip other control chars
        } else {
            out += c;
        }
    }
}

String MQTTManager::getLogJson(uint16_t limit) const {
    if (limit == 0 || limit > MQTT_LOG_CAPACITY) {
        limit = MQTT_LOG_CAPACITY;
    }
    uint16_t n = _logCount;
    if (n > limit) n = limit;

    String out;
    out.reserve((size_t)n * 110u + 48u);
    out += F("{\"count\":");
    out += String(_logCount);
    out += F(",\"capacity\":");
    out += String(MQTT_LOG_CAPACITY);
    out += F(",\"messages\":[");

    for (uint16_t i = 0; i < n; i++) {
        // Newest first
        uint16_t idx = (_logHead + MQTT_LOG_CAPACITY - 1 - i) % MQTT_LOG_CAPACITY;
        const MqttLogEntry& e = _log[idx];
        if (i) out += ',';
        out += F("{\"ms\":");
        out += String(e.ms);
        out += F(",\"dir\":\"");
        out += e.dir;
        out += F("\",\"ok\":");
        out += e.ok ? F("true") : F("false");
        out += F(",\"topic\":\"");
        mqttJsonEscapeAppend(out, e.topic);
        out += F("\",\"payload\":\"");
        mqttJsonEscapeAppend(out, e.payload);
        out += F("\"}");
    }
    out += F("]}");
    return out;
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
