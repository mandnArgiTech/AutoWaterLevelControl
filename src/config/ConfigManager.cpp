/**
 * @file ConfigManager.cpp
 * @brief Implementation of configuration management system
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#include "ConfigManager.h"
#include "../utils/Log.h"

// =============================================================================
// SECTION 1: SINGLETON INSTANCE
// =============================================================================

/**
 * @brief Get singleton instance of ConfigManager
 * @return Reference to the single ConfigManager instance
 */
ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

// =============================================================================
// SECTION 2: CONSTRUCTOR
// =============================================================================

/**
 * @brief Private constructor - initializes member variables
 */
ConfigManager::ConfigManager() 
    : _initialized(false)
    , _fsMounted(false)
    , _configWriteLocked(false) {
    // Initialize with default values
    resetToDefaults();
}

// =============================================================================
// SECTION 3: INITIALIZATION
// =============================================================================

/**
 * @brief Initialize configuration system and LittleFS
 * @return ErrorCode indicating success or failure
 */
ErrorCode ConfigManager::begin() {
    FLM_LOG_INFO("Cfg", "Initializing...");
    
    // Step 1: Initialize filesystem
    if (!initFilesystem()) {
        return ErrorHandler::getInstance().logError(ErrorCode::ERR_FS_MOUNT);
    }
    
    // Step 2: Load configuration from file
    ErrorCode result = loadConfig();
    if (result != ErrorCode::ERR_NONE && result != ErrorCode::ERR_CONFIG_READ) {
        FLM_LOG_WARN("Cfg", "using defaults after init issue");
    }
    
    // Step 3: Validate configuration
    validateConfig();
    
    _initialized = true;
    FLM_LOG_INFO("Cfg", "initialized");
    
    return ErrorCode::ERR_NONE;
}

/**
 * @brief Initialize LittleFS filesystem
 * @return true if successful
 */
bool ConfigManager::initFilesystem() {
    FLM_LOG_INFO("Cfg", "Mounting LittleFS...");
    
    // Step 1: Attempt to mount filesystem
    if (!LittleFS.begin()) {
        FLM_LOG_WARN("Cfg", "mount failed, formatting");
        
        // Step 2: If mount fails, try formatting
        if (!LittleFS.format()) {
            FLM_LOG_ERROR("Cfg", "format failed");
            return false;
        }
        
        // Step 3: Try mounting again after format
        if (!LittleFS.begin()) {
            FLM_LOG_ERROR("Cfg", "mount failed after format");
            return false;
        }
    }
    
    _fsMounted = true;
    
    // Step 4: Print filesystem info
    size_t total, used;
    getFilesystemInfo(total, used);
    FLM_LOG_INFO("Cfg", "FS %u / %u bytes", (unsigned)used, (unsigned)total);
    
    return true;
}

// =============================================================================
// SECTION 4: DEFAULT VALUES
// =============================================================================

/**
 * @brief Reset all configuration to default values
 */
void ConfigManager::resetToDefaults() {
    setDefaultWiFiConfig();
    setDefaultMQTTConfig();
    setDefaultTankConfig();
    setDefaultSensorConfig();
    setDefaultSystemConfig();
    setDefaultMotorConfig();
    
    FLM_LOG_INFO("Cfg", "reset to defaults");
}

void ConfigManager::setDefaultWiFiConfig() {
    _wifiConfig.ssid = DEFAULT_WIFI_SSID;
    _wifiConfig.password = DEFAULT_WIFI_PASSWORD;
    _wifiConfig.hostname = DEFAULT_WIFI_HOSTNAME;
    _wifiConfig.apSsid = DEFAULT_WIFI_AP_SSID;
    _wifiConfig.apPassword = DEFAULT_WIFI_AP_PASSWORD;
    _wifiConfig.connectTimeout = DEFAULT_WIFI_CONNECT_TIMEOUT;
    _wifiConfig.apMode = false;
}

void ConfigManager::setDefaultMQTTConfig() {
    _mqttConfig.enabled = DEFAULT_MQTT_ENABLED;
    _mqttConfig.server = DEFAULT_MQTT_SERVER;
    _mqttConfig.port = DEFAULT_MQTT_PORT;
    _mqttConfig.username = DEFAULT_MQTT_USER;
    _mqttConfig.password = DEFAULT_MQTT_PASSWORD;
    _mqttConfig.clientId = DEFAULT_MQTT_CLIENT_ID;
    _mqttConfig.deviceName = DEFAULT_MQTT_DEVICE_NAME;
    _mqttConfig.topicPrefix = DEFAULT_MQTT_TOPIC_PREFIX;
    _mqttConfig.publishInterval = DEFAULT_MQTT_PUBLISH_INTERVAL;
}

void ConfigManager::setDefaultTankConfig() {
    _tankConfig.type = DEFAULT_TANK_TYPE;
    _tankConfig.diameter = DEFAULT_TANK_DIAMETER;
    _tankConfig.length = 0;
    _tankConfig.width = 0;
    _tankConfig.height = DEFAULT_TANK_HEIGHT;
    _tankConfig.volumeLiters = DEFAULT_TANK_VOLUME_LITERS;
}

void ConfigManager::setDefaultSensorConfig() {
    // Hardware defaults
    _sensorConfig.hardware.type = DEFAULT_SENSOR_TYPE;
    _sensorConfig.hardware.rxPin = DEFAULT_RX_PIN;
    _sensorConfig.hardware.txPin = DEFAULT_TX_PIN;
    _sensorConfig.hardware.trigPin = DEFAULT_TRIG_PIN;
    _sensorConfig.hardware.echoPin = DEFAULT_ECHO_PIN;
    _sensorConfig.hardware.signalPin = DEFAULT_SIGNAL_PIN;
    _sensorConfig.hardware.mountHeightMm = DEFAULT_MOUNT_HEIGHT_MM;
    _sensorConfig.hardware.tankHeightMm = DEFAULT_TANK_HEIGHT;
    _sensorConfig.hardware.invertedLogic = DEFAULT_INVERTED_LOGIC;
    
    // Calibration defaults
    _sensorConfig.offsetMm = DEFAULT_SENSOR_OFFSET;
    _sensorConfig.minDistance = DEFAULT_SENSOR_MIN_DISTANCE;
    _sensorConfig.maxDistance = DEFAULT_SENSOR_MAX_DISTANCE;
    _sensorConfig.samples = DEFAULT_SENSOR_SAMPLES;
    _sensorConfig.readInterval = DEFAULT_SENSOR_INTERVAL;
    
    // Filter defaults for fluid surface stability
    _sensorConfig.filterEnabled = DEFAULT_FILTER_ENABLED;
    _sensorConfig.medianFilterSize = DEFAULT_MEDIAN_FILTER_SIZE;
    _sensorConfig.movingAvgWindow = DEFAULT_MOVING_AVG_WINDOW;
    
    // Kalman filter defaults
    _sensorConfig.kalmanEnabled = DEFAULT_KALMAN_ENABLED;
    _sensorConfig.kalmanProcessNoise = DEFAULT_KALMAN_PROCESS_Q;
    _sensorConfig.kalmanMeasureNoise = DEFAULT_KALMAN_MEASURE_R;
}

void ConfigManager::setDefaultSystemConfig() {
    _systemConfig.deviceName = DEFAULT_DEVICE_NAME;
    _systemConfig.timezoneOffset = DEFAULT_TIMEZONE_OFFSET;
    _systemConfig.ntpServer = DEFAULT_NTP_SERVER;
    _systemConfig.webPort = DEFAULT_WEB_PORT;
    _systemConfig.debugEnabled = DEFAULT_DEBUG_ENABLED;
}

void ConfigManager::setDefaultMotorConfig() {
    _motorConfig.enabled         = DEFAULT_RELAY_ENABLED;
    _motorConfig.pumpOnPercent   = DEFAULT_RELAY_PUMP_ON_PCT;
    _motorConfig.pumpOffPercent  = DEFAULT_RELAY_PUMP_OFF_PCT;
    _motorConfig.maxRunMinutes   = DEFAULT_RELAY_MAX_RUN_MIN;
    _motorConfig.pin             = DEFAULT_RELAY_PIN;
    _motorConfig.activeLow       = DEFAULT_RELAY_ACTIVE_LOW;
    _motorConfig.gsmRxPin        = DEFAULT_GSM_RX_PIN;
    _motorConfig.gsmTxPin        = DEFAULT_GSM_TX_PIN;
    _motorConfig.gsmBaudRate     = DEFAULT_GSM_BAUD;
    _motorConfig.targetPhone     = "";
    _motorConfig.onMessage       = "";
    _motorConfig.offMessage      = "";
    _motorConfig.confirmTimeoutMs = DEFAULT_SMS_CONFIRM_MS;
}

// =============================================================================
// SECTION 5: LOAD CONFIGURATION
// =============================================================================

/**
 * @brief Load configuration from LittleFS JSON file
 * @return ErrorCode indicating success or failure
 */
ErrorCode ConfigManager::loadConfig() {
    // Step 1: Check if file exists
    if (!LittleFS.exists(CONFIG_FILE)) {
        FLM_LOG_INFO("Cfg", "no config file — defaults");
        return ErrorCode::ERR_CONFIG_READ;
    }
    
    // Step 2: Open configuration file
    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file) {
        return ErrorHandler::getInstance().logError(ErrorCode::ERR_CONFIG_READ);
    }
    
    // Step 3: Parse JSON document
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        FLM_LOG_WARN("Cfg", "config parse error, trying backup");
        if (LittleFS.exists(CONFIG_BACKUP_FILE)) {
            File src = LittleFS.open(CONFIG_BACKUP_FILE, "r");
            File dst = LittleFS.open(CONFIG_FILE, "w");
            if (src && dst) {
                uint8_t b[64];
                while (src.available()) {
                    size_t n = src.read(b, sizeof(b));
                    dst.write(b, n);
                    yield();
                }
            }
            if (src) src.close();
            if (dst) dst.close();
            file = LittleFS.open(CONFIG_FILE, "r");
            if (file) {
                error = deserializeJson(doc, file);
                file.close();
            }
        }
        if (error) {
            FLM_LOG_ERROR("Cfg", "config restore failed: %s", error.c_str());
            return ErrorHandler::getInstance().logError(ErrorCode::ERR_CONFIG_PARSE);
        }
        FLM_LOG_INFO("Cfg", "restored config from backup");
    }
    
    // Step 4: Parse each configuration section
    if (doc["wifi"].is<JsonObject>()) {
        parseWiFiConfig(doc["wifi"]);
    }
    if (doc["mqtt"].is<JsonObject>()) {
        parseMQTTConfig(doc["mqtt"]);
    }
    if (doc["tank"].is<JsonObject>()) {
        parseTankConfig(doc["tank"]);
    }
    if (doc["sensor"].is<JsonObject>()) {
        parseSensorConfig(doc["sensor"]);
    }
    if (doc["system"].is<JsonObject>()) {
        parseSystemConfig(doc["system"]);
    }
    if (doc["relay"].is<JsonObject>()) {
        parseMotorConfig(doc["relay"]);
    }
    
    FLM_LOG_INFO("Cfg", "loaded");
    return ErrorCode::ERR_NONE;
}

// =============================================================================
// SECTION 6: SAVE CONFIGURATION
// =============================================================================

bool ConfigManager::tryLockForConfigWrite() {
    noInterrupts();
    if (_configWriteLocked) {
        interrupts();
        return false;
    }
    _configWriteLocked = true;
    interrupts();
    return true;
}

void ConfigManager::unlockConfigWrite() {
    _configWriteLocked = false;
}

ErrorCode ConfigManager::saveConfig() {
    const bool held = _configWriteLocked;
    if (!held) {
        if (!tryLockForConfigWrite()) {
            return ErrorCode::ERR_CONFIG_LOCKED;
        }
    }
    ErrorCode e = saveConfigAtomic();
    if (!held) {
        unlockConfigWrite();
    }
    return e;
}

ErrorCode ConfigManager::saveConfigAtomic() {
    createBackup();

    JsonDocument doc;
    JsonObject jo;
    jo = doc["wifi"].to<JsonObject>();
    serializeWiFiConfig(jo);
    jo = doc["mqtt"].to<JsonObject>();
    serializeMQTTConfig(jo);
    jo = doc["tank"].to<JsonObject>();
    serializeTankConfig(jo);
    jo = doc["sensor"].to<JsonObject>();
    serializeSensorConfig(jo);
    jo = doc["system"].to<JsonObject>();
    serializeSystemConfig(jo);
    jo = doc["relay"].to<JsonObject>();
    serializeMotorConfig(jo);

    LittleFS.remove(CONFIG_TEMP_FILE);
    File file = LittleFS.open(CONFIG_TEMP_FILE, "w");
    if (!file) {
        FLM_LOG_ERROR("Cfg", "open temp config failed (FS full?)");
        return ErrorHandler::getInstance().logError(ErrorCode::ERR_FS_FULL);
    }
    size_t written = serializeJsonPretty(doc, file);
    file.close();
    if (written == 0) {
        LittleFS.remove(CONFIG_TEMP_FILE);
        return ErrorHandler::getInstance().logError(ErrorCode::ERR_CONFIG_WRITE);
    }

    if (LittleFS.exists(CONFIG_FILE)) {
        LittleFS.remove(CONFIG_FILE);
    }
    if (!LittleFS.rename(CONFIG_TEMP_FILE, CONFIG_FILE)) {
        LittleFS.remove(CONFIG_TEMP_FILE);
        (void)restoreBackup();
        FLM_LOG_ERROR("Cfg", "atomic config rename failed");
        return ErrorHandler::getInstance().logError(ErrorCode::ERR_CONFIG_WRITE);
    }

    FLM_LOG_INFO("Cfg", "saved %u bytes", (unsigned)written);
    return ErrorCode::ERR_NONE;
}

// =============================================================================
// SECTION 7: BACKUP AND RESTORE
// =============================================================================

/**
 * @brief Create backup of current configuration
 * @return ErrorCode indicating success or failure
 */
ErrorCode ConfigManager::createBackup() {
    if (!LittleFS.exists(CONFIG_FILE)) {
        return ErrorCode::ERR_NONE; // Nothing to backup
    }
    
    // Copy current config to backup file
    File src = LittleFS.open(CONFIG_FILE, "r");
    if (!src) {
        return ErrorCode::ERR_CONFIG_READ;
    }
    
    File dst = LittleFS.open(CONFIG_BACKUP_FILE, "w");
    if (!dst) {
        src.close();
        return ErrorCode::ERR_CONFIG_WRITE;
    }
    
    uint8_t buf[64];
    while (src.available()) {
        size_t n = src.read(buf, sizeof(buf));
        dst.write(buf, n);
    }
    
    src.close();
    dst.close();
    
    return ErrorCode::ERR_NONE;
}

/**
 * @brief Restore configuration from backup
 * @return ErrorCode indicating success or failure
 */
ErrorCode ConfigManager::restoreBackup() {
    if (!LittleFS.exists(CONFIG_BACKUP_FILE)) {
        return ErrorCode::ERR_CONFIG_READ;
    }
    
    // Copy backup to config file
    File src = LittleFS.open(CONFIG_BACKUP_FILE, "r");
    if (!src) {
        return ErrorCode::ERR_CONFIG_READ;
    }
    
    File dst = LittleFS.open(CONFIG_FILE, "w");
    if (!dst) {
        src.close();
        return ErrorCode::ERR_CONFIG_WRITE;
    }
    
    uint8_t buf[64];
    while (src.available()) {
        size_t n = src.read(buf, sizeof(buf));
        dst.write(buf, n);
    }
    
    src.close();
    dst.close();
    
    // Reload configuration
    return loadConfig();
}

// =============================================================================
// SECTION 8: VALIDATION
// =============================================================================

/**
 * @brief Validate current configuration values
 * @return ErrorCode indicating validation result
 */
void ConfigManager::applyNumericClamps() {
    if (_tankConfig.height < 50.f) _tankConfig.height = 50.f;
    if (_tankConfig.height > 30000.f) _tankConfig.height = 30000.f;
    if (_tankConfig.diameter < 10.f) _tankConfig.diameter = 10.f;
    if (_tankConfig.diameter > 30000.f) _tankConfig.diameter = 30000.f;
    if (_tankConfig.length < 0.f) _tankConfig.length = 0.f;
    if (_tankConfig.width < 0.f) _tankConfig.width = 0.f;
    if (_mqttConfig.port == 0 || _mqttConfig.port > 65535) _mqttConfig.port = DEFAULT_MQTT_PORT;
    if (_systemConfig.webPort == 0 || _systemConfig.webPort > 65535) _systemConfig.webPort = DEFAULT_WEB_PORT;
    if (_mqttConfig.publishInterval < 5000UL) _mqttConfig.publishInterval = 5000UL;
    if (_mqttConfig.publishInterval > 86400000UL) _mqttConfig.publishInterval = 86400000UL;
    if (_sensorConfig.readInterval < 200UL) _sensorConfig.readInterval = 200UL;
    if (_sensorConfig.readInterval > 600000UL) _sensorConfig.readInterval = 600000UL;
    if (_sensorConfig.samples < 1) _sensorConfig.samples = 1;
    if (_sensorConfig.samples > 32) _sensorConfig.samples = 32;
    if (_sensorConfig.medianFilterSize < 1) _sensorConfig.medianFilterSize = 3;
    if (_sensorConfig.medianFilterSize > 15) _sensorConfig.medianFilterSize = 15;
    if (_sensorConfig.movingAvgWindow < 1) _sensorConfig.movingAvgWindow = 1;
    if (_sensorConfig.movingAvgWindow > 50) _sensorConfig.movingAvgWindow = 50;
    if (_sensorConfig.kalmanProcessNoise < 1e-6f) _sensorConfig.kalmanProcessNoise = 1e-6f;
    if (_sensorConfig.kalmanMeasureNoise < 1e-6f) _sensorConfig.kalmanMeasureNoise = 1e-6f;
    if (_sensorConfig.minDistance < 1.f) _sensorConfig.minDistance = 1.f;
    if (_sensorConfig.maxDistance > 10000.f) _sensorConfig.maxDistance = 10000.f;
    if (_motorConfig.pumpOnPercent < 0.f) _motorConfig.pumpOnPercent = 0.f;
    if (_motorConfig.pumpOnPercent > 100.f) _motorConfig.pumpOnPercent = 100.f;
    if (_motorConfig.pumpOffPercent < 0.f) _motorConfig.pumpOffPercent = 0.f;
    if (_motorConfig.pumpOffPercent > 100.f) _motorConfig.pumpOffPercent = 100.f;
    if (_motorConfig.maxRunMinutes < 1) _motorConfig.maxRunMinutes = 1;
    if (_motorConfig.maxRunMinutes > MOTOR_MAX_RUN_MINUTES)
        _motorConfig.maxRunMinutes = MOTOR_MAX_RUN_MINUTES;
    if (_motorConfig.confirmTimeoutMs < 5000u) _motorConfig.confirmTimeoutMs = 5000u;
    if (_motorConfig.confirmTimeoutMs > 120000u) _motorConfig.confirmTimeoutMs = 120000u;
}

ErrorCode ConfigManager::validateConfig() {
    applyNumericClamps();
    bool hasWarnings = false;
    
    // Validate tank dimensions
    if (_tankConfig.height <= 0 || _tankConfig.height > 10000) {
        FLM_LOG_WARN("Cfg", "invalid tank height");
        hasWarnings = true;
    }
    
    if (_tankConfig.type == "circular" && (_tankConfig.diameter <= 0 || _tankConfig.diameter > 10000)) {
        FLM_LOG_WARN("Cfg", "invalid tank diameter");
        hasWarnings = true;
    }
    
    // Validate sensor config
    if (_sensorConfig.offsetMm < 0 || _sensorConfig.offsetMm > _tankConfig.height) {
        FLM_LOG_WARN("Cfg", "sensor offset out of range");
        hasWarnings = true;
    }
    
    // Recalculate tank volume
    _tankConfig.volumeLiters = calculateTankVolume();
    
    if (hasWarnings) {
        return ErrorCode::ERR_CONFIG_VALIDATE;
    }
    
    return ErrorCode::ERR_NONE;
}

// =============================================================================
// SECTION 9: JSON PARSING HELPERS
// =============================================================================

void ConfigManager::parseWiFiConfig(const JsonObject& obj) {
    _wifiConfig.ssid = obj["ssid"] | DEFAULT_WIFI_SSID;
    _wifiConfig.password = obj["password"] | DEFAULT_WIFI_PASSWORD;
    _wifiConfig.hostname = obj["hostname"] | DEFAULT_WIFI_HOSTNAME;
    _wifiConfig.apSsid = obj["apSsid"] | DEFAULT_WIFI_AP_SSID;
    _wifiConfig.apPassword = obj["apPassword"] | DEFAULT_WIFI_AP_PASSWORD;
    _wifiConfig.connectTimeout = obj["connectTimeout"] | DEFAULT_WIFI_CONNECT_TIMEOUT;
    _wifiConfig.apMode = obj["apMode"] | false;
}

void ConfigManager::parseMQTTConfig(const JsonObject& obj) {
    _mqttConfig.enabled = obj["enabled"] | DEFAULT_MQTT_ENABLED;
    _mqttConfig.server = obj["server"] | DEFAULT_MQTT_SERVER;
    _mqttConfig.port = obj["port"] | DEFAULT_MQTT_PORT;
    _mqttConfig.username = obj["username"] | DEFAULT_MQTT_USER;
    _mqttConfig.password = obj["password"] | DEFAULT_MQTT_PASSWORD;
    _mqttConfig.clientId = obj["clientId"] | DEFAULT_MQTT_CLIENT_ID;
    _mqttConfig.deviceName = obj["deviceName"] | DEFAULT_MQTT_DEVICE_NAME;
    _mqttConfig.topicPrefix = obj["topicPrefix"] | DEFAULT_MQTT_TOPIC_PREFIX;
    _mqttConfig.publishInterval = obj["publishInterval"] | DEFAULT_MQTT_PUBLISH_INTERVAL;
}

void ConfigManager::parseTankConfig(const JsonObject& obj) {
    _tankConfig.type = obj["type"] | DEFAULT_TANK_TYPE;
    _tankConfig.diameter = obj["diameter"] | DEFAULT_TANK_DIAMETER;
    _tankConfig.length = obj["length"] | 0.0f;
    _tankConfig.width = obj["width"] | 0.0f;
    _tankConfig.height = obj["height"] | DEFAULT_TANK_HEIGHT;
    _tankConfig.volumeLiters = obj["volumeLiters"] | 0.0f;
    
    // Recalculate volume if not provided
    if (_tankConfig.volumeLiters == 0) {
        _tankConfig.volumeLiters = calculateTankVolume();
    }
}

void ConfigManager::parseSensorConfig(const JsonObject& obj) {
    // Hardware configuration
    if (obj["hardware"].is<JsonObject>()) {
        JsonObject hw = obj["hardware"];
        _sensorConfig.hardware.type = hw["type"] | DEFAULT_SENSOR_TYPE;
        _sensorConfig.hardware.rxPin = hw["rxPin"] | DEFAULT_RX_PIN;
        _sensorConfig.hardware.txPin = hw["txPin"] | DEFAULT_TX_PIN;
        _sensorConfig.hardware.trigPin = hw["trigPin"] | DEFAULT_TRIG_PIN;
        _sensorConfig.hardware.echoPin = hw["echoPin"] | DEFAULT_ECHO_PIN;
        _sensorConfig.hardware.signalPin = hw["signalPin"] | DEFAULT_SIGNAL_PIN;
        _sensorConfig.hardware.mountHeightMm = hw["mountHeightMm"] | DEFAULT_MOUNT_HEIGHT_MM;
        _sensorConfig.hardware.tankHeightMm = hw["tankHeightMm"] | DEFAULT_TANK_HEIGHT;
        _sensorConfig.hardware.invertedLogic = hw["invertedLogic"] | DEFAULT_INVERTED_LOGIC;
    } else {
        // Legacy format - use top-level type field if present
        _sensorConfig.hardware.type = obj["type"] | DEFAULT_SENSOR_TYPE;
        _sensorConfig.hardware.rxPin = DEFAULT_RX_PIN;
        _sensorConfig.hardware.txPin = DEFAULT_TX_PIN;
        _sensorConfig.hardware.trigPin = DEFAULT_TRIG_PIN;
        _sensorConfig.hardware.echoPin = DEFAULT_ECHO_PIN;
        _sensorConfig.hardware.signalPin = DEFAULT_SIGNAL_PIN;
        _sensorConfig.hardware.mountHeightMm = DEFAULT_MOUNT_HEIGHT_MM;
        _sensorConfig.hardware.tankHeightMm = DEFAULT_TANK_HEIGHT;
        _sensorConfig.hardware.invertedLogic = DEFAULT_INVERTED_LOGIC;
    }
    
    // Calibration settings
    _sensorConfig.offsetMm = obj["offsetMm"] | DEFAULT_SENSOR_OFFSET;
    _sensorConfig.minDistance = obj["minDistance"] | DEFAULT_SENSOR_MIN_DISTANCE;
    _sensorConfig.maxDistance = obj["maxDistance"] | DEFAULT_SENSOR_MAX_DISTANCE;
    _sensorConfig.samples = obj["samples"] | DEFAULT_SENSOR_SAMPLES;
    _sensorConfig.readInterval = obj["readInterval"] | DEFAULT_SENSOR_INTERVAL;
    
    // Filter settings
    _sensorConfig.filterEnabled = obj["filterEnabled"] | DEFAULT_FILTER_ENABLED;
    _sensorConfig.medianFilterSize = obj["medianFilterSize"] | DEFAULT_MEDIAN_FILTER_SIZE;
    _sensorConfig.movingAvgWindow = obj["movingAvgWindow"] | DEFAULT_MOVING_AVG_WINDOW;
    
    // Kalman filter settings
    _sensorConfig.kalmanEnabled = obj["kalmanEnabled"] | DEFAULT_KALMAN_ENABLED;
    _sensorConfig.kalmanProcessNoise = obj["kalmanProcessNoise"] | DEFAULT_KALMAN_PROCESS_Q;
    _sensorConfig.kalmanMeasureNoise = obj["kalmanMeasureNoise"] | DEFAULT_KALMAN_MEASURE_R;
}

void ConfigManager::parseSystemConfig(const JsonObject& obj) {
    _systemConfig.deviceName = obj["deviceName"] | DEFAULT_DEVICE_NAME;
    _systemConfig.timezoneOffset = obj["timezoneOffset"] | DEFAULT_TIMEZONE_OFFSET;
    _systemConfig.ntpServer = obj["ntpServer"] | DEFAULT_NTP_SERVER;
    _systemConfig.webPort = obj["webPort"] | DEFAULT_WEB_PORT;
    _systemConfig.debugEnabled = obj["debugEnabled"] | DEFAULT_DEBUG_ENABLED;
}

void ConfigManager::parseMotorConfig(const JsonObject& obj) {
    _motorConfig.enabled = obj["enabled"] | DEFAULT_RELAY_ENABLED;
    _motorConfig.pin = obj["pin"] | DEFAULT_RELAY_PIN;
    _motorConfig.activeLow = obj["activeLow"] | DEFAULT_RELAY_ACTIVE_LOW;
    _motorConfig.pumpOnPercent = obj["pumpOnPercent"] | DEFAULT_RELAY_PUMP_ON_PCT;
    _motorConfig.pumpOffPercent = obj["pumpOffPercent"] | DEFAULT_RELAY_PUMP_OFF_PCT;
    _motorConfig.maxRunMinutes = obj["maxRunMinutes"] | DEFAULT_RELAY_MAX_RUN_MIN;
    if (obj["gsm"].is<JsonObject>()) {
        JsonObject g = obj["gsm"];
        _motorConfig.gsmRxPin = g["rxPin"] | DEFAULT_GSM_RX_PIN;
        _motorConfig.gsmTxPin = g["txPin"] | DEFAULT_GSM_TX_PIN;
        _motorConfig.gsmBaudRate = g["baudRate"] | DEFAULT_GSM_BAUD;
        _motorConfig.targetPhone = g["targetPhone"] | "";
        _motorConfig.onMessage = g["onMessage"] | "";
        _motorConfig.offMessage = g["offMessage"] | "";
    } else {
        _motorConfig.gsmRxPin = obj["gsmRxPin"] | DEFAULT_GSM_RX_PIN;
        _motorConfig.gsmTxPin = obj["gsmTxPin"] | DEFAULT_GSM_TX_PIN;
        _motorConfig.gsmBaudRate = obj["gsmBaudRate"] | DEFAULT_GSM_BAUD;
        _motorConfig.targetPhone = obj["targetPhone"] | "";
        _motorConfig.onMessage = obj["onMessage"] | "";
        _motorConfig.offMessage = obj["offMessage"] | "";
    }
    _motorConfig.confirmTimeoutMs = obj["confirmTimeoutMs"] | DEFAULT_SMS_CONFIRM_MS;
}

// =============================================================================
// SECTION 10: JSON SERIALIZATION HELPERS
// =============================================================================

void ConfigManager::serializeWiFiConfig(JsonObject& obj) const {
    obj["ssid"] = _wifiConfig.ssid;
    obj["password"] = _wifiConfig.password;
    obj["hostname"] = _wifiConfig.hostname;
    obj["apSsid"] = _wifiConfig.apSsid;
    obj["apPassword"] = _wifiConfig.apPassword;
    obj["connectTimeout"] = _wifiConfig.connectTimeout;
    obj["apMode"] = _wifiConfig.apMode;
}

void ConfigManager::serializeMQTTConfig(JsonObject& obj) const {
    obj["enabled"] = _mqttConfig.enabled;
    obj["server"] = _mqttConfig.server;
    obj["port"] = _mqttConfig.port;
    obj["username"] = _mqttConfig.username;
    obj["password"] = _mqttConfig.password;
    obj["clientId"] = _mqttConfig.clientId;
    obj["deviceName"] = _mqttConfig.deviceName;
    obj["topicPrefix"] = _mqttConfig.topicPrefix;
    obj["publishInterval"] = _mqttConfig.publishInterval;
}

void ConfigManager::serializeTankConfig(JsonObject& obj) const {
    obj["type"] = _tankConfig.type;
    obj["diameter"] = _tankConfig.diameter;
    obj["length"] = _tankConfig.length;
    obj["width"] = _tankConfig.width;
    obj["height"] = _tankConfig.height;
    obj["volumeLiters"] = _tankConfig.volumeLiters;
}

void ConfigManager::serializeSensorConfig(JsonObject& obj) const {
    // Hardware configuration
    JsonObject hw = obj["hardware"].to<JsonObject>();
    hw["type"] = _sensorConfig.hardware.type;
    hw["rxPin"] = _sensorConfig.hardware.rxPin;
    hw["txPin"] = _sensorConfig.hardware.txPin;
    hw["trigPin"] = _sensorConfig.hardware.trigPin;
    hw["echoPin"] = _sensorConfig.hardware.echoPin;
    hw["signalPin"] = _sensorConfig.hardware.signalPin;
    hw["mountHeightMm"] = _sensorConfig.hardware.mountHeightMm;
    hw["tankHeightMm"] = _sensorConfig.hardware.tankHeightMm;
    hw["invertedLogic"] = _sensorConfig.hardware.invertedLogic;
    
    // Calibration settings
    obj["offsetMm"] = _sensorConfig.offsetMm;
    obj["minDistance"] = _sensorConfig.minDistance;
    obj["maxDistance"] = _sensorConfig.maxDistance;
    obj["samples"] = _sensorConfig.samples;
    obj["readInterval"] = _sensorConfig.readInterval;
    
    // Filter settings
    obj["filterEnabled"] = _sensorConfig.filterEnabled;
    obj["medianFilterSize"] = _sensorConfig.medianFilterSize;
    obj["movingAvgWindow"] = _sensorConfig.movingAvgWindow;
    
    // Kalman filter settings
    obj["kalmanEnabled"] = _sensorConfig.kalmanEnabled;
    obj["kalmanProcessNoise"] = _sensorConfig.kalmanProcessNoise;
    obj["kalmanMeasureNoise"] = _sensorConfig.kalmanMeasureNoise;
}

void ConfigManager::serializeSystemConfig(JsonObject& obj) const {
    obj["deviceName"] = _systemConfig.deviceName;
    obj["timezoneOffset"] = _systemConfig.timezoneOffset;
    obj["ntpServer"] = _systemConfig.ntpServer;
    obj["webPort"] = _systemConfig.webPort;
    obj["debugEnabled"] = _systemConfig.debugEnabled;
}

void ConfigManager::serializeMotorConfig(JsonObject& obj) const {
    obj["enabled"] = _motorConfig.enabled;
    obj["pin"] = _motorConfig.pin;
    obj["activeLow"] = _motorConfig.activeLow;
    obj["pumpOnPercent"] = _motorConfig.pumpOnPercent;
    obj["pumpOffPercent"] = _motorConfig.pumpOffPercent;
    obj["maxRunMinutes"] = _motorConfig.maxRunMinutes;
    obj["confirmTimeoutMs"] = _motorConfig.confirmTimeoutMs;
    JsonObject g = obj["gsm"].to<JsonObject>();
    g["rxPin"] = _motorConfig.gsmRxPin;
    g["txPin"] = _motorConfig.gsmTxPin;
    g["baudRate"] = _motorConfig.gsmBaudRate;
    g["targetPhone"] = _motorConfig.targetPhone;
    g["onMessage"] = _motorConfig.onMessage;
    g["offMessage"] = _motorConfig.offMessage;
}

// =============================================================================
// SECTION 11: JSON ACCESSORS
// =============================================================================

/**
 * @brief Get entire configuration as JSON string
 * @return JSON string
 */
String ConfigManager::getConfigJson() const {
    JsonDocument doc;
    
    JsonObject wifiObj = doc["wifi"].to<JsonObject>();
    serializeWiFiConfig(wifiObj);
    
    JsonObject mqttObj = doc["mqtt"].to<JsonObject>();
    serializeMQTTConfig(mqttObj);
    
    JsonObject tankObj = doc["tank"].to<JsonObject>();
    serializeTankConfig(tankObj);
    
    JsonObject sensorObj = doc["sensor"].to<JsonObject>();
    serializeSensorConfig(sensorObj);
    
    JsonObject systemObj = doc["system"].to<JsonObject>();
    serializeSystemConfig(systemObj);
    
    JsonObject relayObj = doc["relay"].to<JsonObject>();
    serializeMotorConfig(relayObj);
    
    String output;
    serializeJson(doc, output);
    return output;
}

/**
 * @brief Set configuration from JSON string
 * @param json JSON configuration string
 * @return ErrorCode indicating success or failure
 */
ErrorCode ConfigManager::setConfigFromJson(const String& json) {
    if (json.length() > FLM_MAX_CONFIG_JSON_BYTES) {
        return ErrorHandler::getInstance().logError(ErrorCode::ERR_WEB_REQUEST, "config body too large");
    }
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        return ErrorHandler::getInstance().logError(ErrorCode::ERR_CONFIG_PARSE);
    }
    
    // Parse all sections if present
    if (doc["wifi"].is<JsonObject>()) {
        parseWiFiConfig(doc["wifi"]);
    }
    if (doc["mqtt"].is<JsonObject>()) {
        parseMQTTConfig(doc["mqtt"]);
    }
    if (doc["tank"].is<JsonObject>()) {
        parseTankConfig(doc["tank"]);
    }
    if (doc["sensor"].is<JsonObject>()) {
        parseSensorConfig(doc["sensor"]);
    }
    if (doc["system"].is<JsonObject>()) {
        parseSystemConfig(doc["system"]);
    }
    if (doc["relay"].is<JsonObject>()) {
        parseMotorConfig(doc["relay"]);
    }
    
    return validateConfig();
}

/**
 * @brief Update specific section from JSON
 * @param section Section name
 * @param json JSON string for that section
 * @return ErrorCode indicating success or failure
 */
ErrorCode ConfigManager::updateSection(const String& section, const String& json) {
    if (json.length() > FLM_MAX_CONFIG_JSON_BYTES) {
        return ErrorHandler::getInstance().logError(ErrorCode::ERR_WEB_REQUEST, "section body too large");
    }
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        return ErrorHandler::getInstance().logError(ErrorCode::ERR_CONFIG_PARSE);
    }
    
    if (section == "wifi") {
        parseWiFiConfig(doc.as<JsonObject>());
    } else if (section == "mqtt") {
        parseMQTTConfig(doc.as<JsonObject>());
    } else if (section == "tank") {
        parseTankConfig(doc.as<JsonObject>());
    } else if (section == "sensor") {
        parseSensorConfig(doc.as<JsonObject>());
    } else if (section == "system") {
        parseSystemConfig(doc.as<JsonObject>());
    } else if (section == "relay") {
        parseMotorConfig(doc.as<JsonObject>());
    } else {
        return ErrorCode::ERR_CONFIG_VALIDATE;
    }
    
    return validateConfig();
}

/**
 * @brief Get specific section as JSON
 * @param section Section name
 * @return JSON string for section
 */
String ConfigManager::getSectionJson(const String& section) const {
    JsonDocument doc;
    
    if (section == "wifi") {
        JsonObject obj = doc.to<JsonObject>();
        serializeWiFiConfig(obj);
    } else if (section == "mqtt") {
        JsonObject obj = doc.to<JsonObject>();
        serializeMQTTConfig(obj);
    } else if (section == "tank") {
        JsonObject obj = doc.to<JsonObject>();
        serializeTankConfig(obj);
    } else if (section == "sensor") {
        JsonObject obj = doc.to<JsonObject>();
        serializeSensorConfig(obj);
    } else if (section == "system") {
        JsonObject obj = doc.to<JsonObject>();
        serializeSystemConfig(obj);
    } else if (section == "relay") {
        JsonObject obj = doc.to<JsonObject>();
        serializeMotorConfig(obj);
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

// =============================================================================
// SECTION 12: UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Get filesystem info
 * @param totalBytes Total bytes (output)
 * @param usedBytes Used bytes (output)
 */
void ConfigManager::getFilesystemInfo(size_t& totalBytes, size_t& usedBytes) const {
    FSInfo fs_info;
    LittleFS.info(fs_info);
    totalBytes = fs_info.totalBytes;
    usedBytes = fs_info.usedBytes;
}

/**
 * @brief Calculate tank volume in liters based on dimensions
 * @return Volume in liters
 */
float ConfigManager::calculateTankVolume() const {
    float volumeMm3 = 0;
    
    if (_tankConfig.type == "circular") {
        // Volume = π * r² * h
        float radius = _tankConfig.diameter / 2.0f;
        volumeMm3 = PI * radius * radius * _tankConfig.height;
    } else if (_tankConfig.type == "rectangular") {
        // Volume = l * w * h
        volumeMm3 = _tankConfig.length * _tankConfig.width * _tankConfig.height;
    }
    
    // Convert mm³ to liters (1 liter = 1,000,000 mm³)
    return volumeMm3 / 1000000.0f;
}

