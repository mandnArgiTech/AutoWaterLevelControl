/**
 * @file WiFiManager.cpp
 * @brief Implementation of WiFi connection management with OTA
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#include "WiFiManager.h"
#include "../version.h"
#include "../utils/Log.h"
#include "../utils/BootDiagnostics.h"

// =============================================================================
// SECTION 1: SINGLETON INSTANCE
// =============================================================================

/**
 * @brief Get singleton instance of WiFiManager
 * @return Reference to the single WiFiManager instance
 */
WiFiManager& WiFiManager::getInstance() {
    static WiFiManager instance;
    return instance;
}

// =============================================================================
// SECTION 2: CONSTRUCTOR
// =============================================================================

/**
 * @brief Private constructor - initializes member variables
 */
WiFiManager::WiFiManager()
    : _state(WifiMgrState::DISCONNECTED)
    , _stateCallback(nullptr)
    , _otaPrepareCallback(nullptr)
    , _otaErrorCallback(nullptr)
    , _lastConnectAttempt(0)
    , _connectStartMs(0)
    , _reconnectInterval(30000)  // 30 seconds
    , _reconnectCount(0)
    , _otaInProgress(false)
    , _dnsServer(nullptr) {
}

// =============================================================================
// SECTION 3: INITIALIZATION
// =============================================================================

/**
 * @brief Initialize WiFi manager
 * @return ErrorCode indicating success or failure
 */
ErrorCode WiFiManager::begin() {
    FLM_LOG_INFO("WiFi", "Initializing...");
    
    // Step 1: Print device info
    FLM_LOG_DEBUG("WiFi", "device %s AP %s", getDeviceId().c_str(), getAPSSID().c_str());
    
    // Step 2: Set WiFi mode
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);  // Don't save credentials to flash
    
    // Step 3: Set hostname
    WiFiConfig& config = ConfigManager::getInstance().getWiFiConfig();
    WiFi.hostname(config.hostname);
    
    // Step 4: Check if forced AP mode or no SSID configured
    if (config.apMode || config.ssid.length() == 0) {
        FLM_LOG_INFO("WiFi", "no STA config — AP mode");
        ErrorCode result = startAP();
        if (result != ErrorCode::ERR_NONE) {
            return result;
        }
    } else {
        // Step 5: Try to connect to configured network
        ErrorCode result = connect();
        if (result != ErrorCode::ERR_NONE) {
            // Fall back to AP mode if connection fails
            FLM_LOG_WARN("WiFi", "STA failed — AP mode");
            startAP();
        }
    }
    
    // Step 6: Setup OTA
    setupOTA();
    
    FLM_LOG_INFO("WiFi", "ready");
    
    return ErrorCode::ERR_NONE;
}

// =============================================================================
// SECTION 4: CONNECTION MANAGEMENT
// =============================================================================

/**
 * @brief Start a connection attempt without waiting for the result.
 * Result is evaluated by finishConnect() — either from the blocking boot-time
 * connect() or from the non-blocking checkConnection() state machine.
 */
ErrorCode WiFiManager::beginConnect() {
    WiFiConfig& config = ConfigManager::getInstance().getWiFiConfig();
    
    if (config.ssid.length() == 0) {
        FLM_LOG_WARN("WiFi", "no SSID");
        return ErrorHandler::getInstance().logError(ErrorCode::ERR_WIFI_NO_SSID);
    }
    
    FLM_LOG_INFO("WiFi", "connecting to %s", config.ssid.c_str());
    setState(WifiMgrState::CONNECTING);
    
    // Stop AP if running - properly stop DNS server first
    if (_dnsServer) {
        _dnsServer->stop();  // Stop before delete to prevent leaks
        delete _dnsServer;
        _dnsServer = nullptr;
    }
    WiFi.softAPdisconnect(true);
    
    if (WiFi.isConnected()) {
        WiFi.disconnect();
        delay(100);
    }
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.ssid.c_str(), config.password.c_str());
    _lastConnectAttempt = millis();
    _connectStartMs = millis();
    
    return ErrorCode::ERR_NONE;
}

/**
 * @brief Evaluate the result of a connection attempt.
 */
ErrorCode WiFiManager::finishConnect() {
    if (WiFi.isConnected()) {
        setState(WifiMgrState::CONNECTED);
        _reconnectCount = 0;
        
        FLM_LOG_INFO("WiFi", "connected IP %s RSSI %d", getIP().c_str(), getRSSI());
        setupMDNS();
        return ErrorCode::ERR_NONE;
    }
    
    setState(WifiMgrState::CONNECTION_FAILED);
    _reconnectCount++;
    
    return ErrorHandler::getInstance().logError(ErrorCode::ERR_WIFI_CONNECT,
        "SSID: " + ConfigManager::getInstance().getWiFiConfig().ssid);
}

/**
 * @brief Connect to configured WiFi network. Blocking — boot-time use only,
 * so services start with WiFi already up. Runtime reconnects go through the
 * non-blocking path in checkConnection().
 */
ErrorCode WiFiManager::connect() {
    ErrorCode result = beginConnect();
    if (result != ErrorCode::ERR_NONE) {
        return result;
    }
    
    uint32_t timeout = ConfigManager::getInstance().getWiFiConfig().connectTimeout;
    unsigned long startTime = millis();
    while (!WiFi.isConnected() && (millis() - startTime) < timeout) {
        delay(100);
        ESP.wdtFeed();
        yield();  // Allow background tasks
    }
    
    return finishConnect();
}

// =============================================================================
// SECTION 5: ACCESS POINT MODE
// =============================================================================

/**
 * @brief Start Access Point mode with captive portal
 * @return ErrorCode indicating success or failure
 */
ErrorCode WiFiManager::startAP() {
    String apSSID = getAPSSID();
    WiFiConfig& config = ConfigManager::getInstance().getWiFiConfig();
    
    FLM_LOG_INFO("WiFi", "AP %s @ %s", apSSID.c_str(), AP_IP.toString().c_str());
    
    // Step 1: Set mode to AP+STA (allows scanning while in AP mode)
    WiFi.mode(WIFI_AP_STA);
    
    // Step 2: Configure and start AP with dynamic SSID
    bool success;
    if (config.apPassword.length() >= 8) {
        success = WiFi.softAP(apSSID.c_str(), config.apPassword.c_str());
    } else {
        success = WiFi.softAP(apSSID.c_str());  // Open network
    }
    
    if (!success) {
        return ErrorHandler::getInstance().logError(ErrorCode::ERR_WIFI_AP_FAILED);
    }
    
    // Step 3: Configure AP IP
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
    
    // Step 4: Setup captive portal DNS
    setupCaptivePortal();
    
    setState(WifiMgrState::AP_MODE);
    
    FLM_LOG_INFO("WiFi", "AP %s — http://%s (pwd %s)", apSSID.c_str(),
                 AP_IP.toString().c_str(),
                 config.apPassword.length() >= 8 ? "set (not logged)" : "open");
    
    // Step 5: Setup mDNS for AP mode
    setupMDNS();
    
    return ErrorCode::ERR_NONE;
}

/**
 * @brief Setup DNS server for captive portal
 */
void WiFiManager::setupCaptivePortal() {
    if (_dnsServer) {
        _dnsServer->stop();
        delete _dnsServer;
    }
    
    _dnsServer = new DNSServer();
    
    // Redirect all DNS queries to AP IP (captive portal)
    _dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    _dnsServer->start(DNS_PORT, "*", AP_IP);
    
    FLM_LOG_DEBUG("WiFi", "captive DNS");
}

// =============================================================================
// SECTION 6: OTA SETUP
// =============================================================================

/**
 * @brief Setup OTA (Over-The-Air) updates
 */
void WiFiManager::setupOTA() {
    // Step 1: Set OTA hostname
    ArduinoOTA.setHostname(ConfigManager::getInstance().getWiFiConfig().hostname.c_str());
    
    // Step 2: Set OTA password (optional)
    // ArduinoOTA.setPassword("admin");
    
    // Step 3: OTA callbacks
    ArduinoOTA.onStart([this]() {
        _otaInProgress = true;
        const char* type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
        FLM_LOG_INFO("OTA", "start %s", type);
        // Mark early — esp8266 often reboots from the updater before onEnd runs.
        BootDiagnostics::getInstance().markIntentionalRestart(
            (ArduinoOTA.getCommand() == U_FLASH) ? "arduino_ota_firmware" : "arduino_ota_filesystem");
        if (_otaPrepareCallback) {
            _otaPrepareCallback();
        }
        FLM_LOG_DEBUG("OTA", "heap %u", (unsigned)ESP.getFreeHeap());
    });
    
    ArduinoOTA.onEnd([this]() {
        _otaInProgress = false;
        FLM_LOG_INFO("OTA", "complete");
        BootDiagnostics::getInstance().markIntentionalRestart("arduino_ota");
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static int lastPercent = -1;
        int percent = (progress / (total / 100));
        if (percent != lastPercent && percent % 10 == 0) {
            FLM_LOG_DEBUG("OTA", "%u%%", (unsigned)percent);
            lastPercent = percent;
        }
    });
    
    ArduinoOTA.onError([this](ota_error_t error) {
        _otaInProgress = false;
        FLM_LOG_ERROR("OTA", "error %u", (unsigned)error);
        switch (error) {
            case OTA_AUTH_ERROR:
            case OTA_BEGIN_ERROR:
            case OTA_CONNECT_ERROR:
            case OTA_RECEIVE_ERROR:
            case OTA_END_ERROR:
                break;
        }
        if (_otaErrorCallback) {
            _otaErrorCallback();
        }
    });
    
    // Step 4: Start OTA
    ArduinoOTA.begin();
    FLM_LOG_INFO("WiFi", "OTA ready");
}

// =============================================================================
// SECTION 7: LOOP PROCESSING
// =============================================================================

/**
 * @brief Process WiFi tasks (call in loop)
 */
void WiFiManager::loop() {
    // Step 1: Handle OTA
    ArduinoOTA.handle();
    
    // Step 2: Handle DNS for captive portal
    if (_dnsServer && _state == WifiMgrState::AP_MODE) {
        _dnsServer->processNextRequest();
    }
    
    // Step 3: Update mDNS
    MDNS.update();
    
    // Step 4: Check connection status (only if not in OTA)
    if (!_otaInProgress) {
        checkConnection();
    }
}

/**
 * @brief Check and handle reconnection.
 * Fully non-blocking: connection attempts started here are polled across
 * loop() iterations so the web server, MQTT, and motor safety loop keep
 * running while WiFi is negotiating.
 */
void WiFiManager::checkConnection() {
    // Skip if in AP mode
    if (_state == WifiMgrState::AP_MODE) {
        return;
    }
    
    // Poll an in-progress connection attempt
    if (_state == WifiMgrState::CONNECTING) {
        uint32_t timeout = ConfigManager::getInstance().getWiFiConfig().connectTimeout;
        if (WiFi.isConnected() || (millis() - _connectStartMs) >= timeout) {
            finishConnect();
        }
        return;
    }
    
    // Check if connection lost
    if (_state == WifiMgrState::CONNECTED && !WiFi.isConnected()) {
        FLM_LOG_WARN("WiFi", "connection lost");
        setState(WifiMgrState::DISCONNECTED);
        ErrorHandler::getInstance().logError(ErrorCode::ERR_WIFI_DISCONNECTED);
    }
    
    if (_state == WifiMgrState::DISCONNECTED || _state == WifiMgrState::CONNECTION_FAILED) {
        // SDK auto-reconnect may have restored the link on its own
        if (WiFi.isConnected()) {
            FLM_LOG_INFO("WiFi", "link restored by auto-reconnect");
            finishConnect();
            return;
        }
        
        // Attempt reconnection (non-blocking)
        if (millis() - _lastConnectAttempt > _reconnectInterval) {
            if (_reconnectCount < MAX_RECONNECT_ATTEMPTS) {
                FLM_LOG_INFO("WiFi", "reconnecting...");
                beginConnect();
            } else {
                // Max reconnection attempts reached, switch to AP mode
                FLM_LOG_WARN("WiFi", "max retries — AP mode");
                startAP();
                _reconnectCount = 0;
            }
        }
    }
}

// =============================================================================
// SECTION 8: MDNS SETUP
// =============================================================================

/**
 * @brief Setup mDNS responder
 */
void WiFiManager::setupMDNS() {
    WiFiConfig& config = ConfigManager::getInstance().getWiFiConfig();
    
    if (MDNS.begin(config.hostname.c_str())) {
        MDNS.addService("http", "tcp", ConfigManager::getInstance().getSystemConfig().webPort);
        FLM_LOG_INFO("WiFi", "mDNS http://%s.local", config.hostname.c_str());
    } else {
        FLM_LOG_WARN("WiFi", "mDNS failed");
    }
}

// =============================================================================
// SECTION 9: STATE MANAGEMENT
// =============================================================================

/**
 * @brief Update state and call callback
 * @param newState New WiFi state
 */
void WiFiManager::setState(WifiMgrState newState) {
    if (_state != newState) {
        _state = newState;
        if (_stateCallback) {
            _stateCallback(_state);
        }
    }
}

/**
 * @brief Set state change callback
 * @param callback Function to call on state change
 */
void WiFiManager::setStateCallback(WiFiCallback callback) {
    _stateCallback = callback;
}

// =============================================================================
// SECTION 10: STATUS ACCESSORS
// =============================================================================

/**
 * @brief Get IP address
 * @return IP address string
 */
String WiFiManager::getIP() const {
    if (WiFi.isConnected()) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

/**
 * @brief Get AP IP address
 * @return AP IP address string
 */
String WiFiManager::getAPIP() const {
    return WiFi.softAPIP().toString();
}

/**
 * @brief Get MAC address
 * @return MAC address string
 */
String WiFiManager::getMAC() const {
    return WiFi.macAddress();
}

/**
 * @brief Get device ID (chip ID in hex)
 * @return Device ID string
 */
String WiFiManager::getDeviceId() const {
    char id[9];
    snprintf(id, sizeof(id), "%08X", ESP.getChipId());
    return String(id);
}

/**
 * @brief Get dynamic AP SSID
 * @return AP SSID string (FluidLM_<chipid>)
 */
String WiFiManager::getAPSSID() const {
    return "FluidLM_" + getDeviceId();
}

/**
 * @brief Get WiFi signal strength (RSSI)
 * @return RSSI value in dBm
 */
int WiFiManager::getRSSI() const {
    if (WiFi.isConnected()) {
        return WiFi.RSSI();
    }
    return 0;
}

/**
 * @brief Get signal quality percentage
 * @return Signal quality 0-100%
 */
int WiFiManager::getSignalQuality() const {
    int rssi = getRSSI();
    if (rssi == 0) return 0;
    
    // Convert RSSI to percentage
    // -50 dBm or better = 100%
    // -100 dBm or worse = 0%
    if (rssi >= -50) return 100;
    if (rssi <= -100) return 0;
    return 2 * (rssi + 100);
}

/**
 * @brief Get connected SSID
 * @return SSID string
 */
String WiFiManager::getSSID() const {
    return WiFi.SSID();
}

// =============================================================================
// SECTION 11: NETWORK SCANNING
// =============================================================================

/**
 * @brief Scan for available networks
 * @return JSON array of networks
 */
String WiFiManager::scanNetworks() {
    FLM_LOG_DEBUG("WiFi", "scanning...");
    
    int numNetworks = WiFi.scanNetworks();
    const int maxNet = 10;
    int n = (numNetworks > maxNet) ? maxNet : numNetworks;

    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();

    for (int i = 0; i < n; i++) {
        yield();
        JsonObject network = networks.add<JsonObject>();
        network["ssid"] = WiFi.SSID(i);
        network["rssi"] = WiFi.RSSI(i);
        network["encryption"] = (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? "open" : "secured";
        network["channel"] = WiFi.channel(i);
    }
    
    doc["count"] = n;
    doc["totalScanned"] = numNetworks;
    
    WiFi.scanDelete();  // Free memory
    
    String output;
    serializeJson(doc, output);
    return output;
}

// =============================================================================
// SECTION 12: JSON STATUS
// =============================================================================

/**
 * @brief Get WiFi status as JSON string
 * @return JSON string with WiFi status
 */
String WiFiManager::getStatusJson() const {
    JsonDocument doc;
    
    doc["state"] = stateToString(_state);
    doc["connected"] = isConnected();
    doc["apMode"] = isAPMode();
    doc["deviceId"] = getDeviceId();
    
    if (isConnected()) {
        doc["ssid"] = getSSID();
        doc["ip"] = getIP();
        doc["rssi"] = getRSSI();
        doc["signalQuality"] = getSignalQuality();
    }
    
    if (isAPMode()) {
        doc["apIP"] = getAPIP();
        doc["apSSID"] = getAPSSID();
    }
    
    doc["mac"] = getMAC();
    doc["hostname"] = ConfigManager::getInstance().getWiFiConfig().hostname;
    doc["otaEnabled"] = true;
    doc["otaInProgress"] = _otaInProgress;
    
    String output;
    serializeJson(doc, output);
    return output;
}

/**
 * @brief Convert state to string
 * @param state WifiMgrState value
 * @return String representation
 */
String WiFiManager::stateToString(WifiMgrState state) {
    switch (state) {
        case WifiMgrState::DISCONNECTED:       return "disconnected";
        case WifiMgrState::CONNECTING:         return "connecting";
        case WifiMgrState::CONNECTED:          return "connected";
        case WifiMgrState::AP_MODE:            return "ap_mode";
        case WifiMgrState::CONNECTION_FAILED:  return "connection_failed";
        default:                               return "unknown";
    }
}
