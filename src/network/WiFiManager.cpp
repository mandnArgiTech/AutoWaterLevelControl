/**
 * @file WiFiManager.cpp
 * @brief Implementation of WiFi connection management with OTA
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#include "WiFiManager.h"
#include "../version.h"

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
    : _initialized(false)
    , _state(WifiMgrState::DISCONNECTED)
    , _stateCallback(nullptr)
    , _otaPrepareCallback(nullptr)
    , _otaErrorCallback(nullptr)
    , _lastConnectAttempt(0)
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
    Serial.println(F("[WiFiManager] Initializing..."));
    
    // Step 1: Print device info
    Serial.printf("[WiFiManager] Device ID: %s\n", getDeviceId().c_str());
    Serial.printf("[WiFiManager] AP SSID: %s\n", getAPSSID().c_str());
    Serial.printf("[WiFiManager] MAC: %s\n", getMAC().c_str());
    
    // Step 2: Set WiFi mode
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);  // Don't save credentials to flash
    
    // Step 3: Set hostname
    WiFiConfig& config = ConfigManager::getInstance().getWiFiConfig();
    WiFi.hostname(config.hostname);
    
    // Step 4: Check if forced AP mode or no SSID configured
    if (config.apMode || config.ssid.length() == 0) {
        Serial.println(F("[WiFiManager] No WiFi configured, starting AP mode"));
        ErrorCode result = startAP();
        if (result != ErrorCode::ERR_NONE) {
            return result;
        }
    } else {
        // Step 5: Try to connect to configured network
        ErrorCode result = connect();
        if (result != ErrorCode::ERR_NONE) {
            // Fall back to AP mode if connection fails
            Serial.println(F("[WiFiManager] Connection failed, starting AP mode"));
            startAP();
        }
    }
    
    // Step 6: Setup OTA
    setupOTA();
    
    _initialized = true;
    Serial.println(F("[WiFiManager] Initialized"));
    
    return ErrorCode::ERR_NONE;
}

// =============================================================================
// SECTION 4: CONNECTION MANAGEMENT
// =============================================================================

/**
 * @brief Connect to configured WiFi network
 * @return ErrorCode indicating success or failure
 */
ErrorCode WiFiManager::connect() {
    WiFiConfig& config = ConfigManager::getInstance().getWiFiConfig();
    
    // Step 1: Check if SSID is configured
    if (config.ssid.length() == 0) {
        Serial.println(F("[WiFiManager] No SSID configured"));
        return ErrorHandler::getInstance().logError(ErrorCode::ERR_WIFI_NO_SSID);
    }
    
    Serial.printf("[WiFiManager] Connecting to: %s\n", config.ssid.c_str());
    setState(WifiMgrState::CONNECTING);
    
    // Step 2: Stop AP if running - properly stop DNS server first
    if (_dnsServer) {
        _dnsServer->stop();  // Stop before delete to prevent leaks
        delete _dnsServer;
        _dnsServer = nullptr;
    }
    WiFi.softAPdisconnect(true);
    
    // Step 3: Disconnect if already connected
    if (WiFi.isConnected()) {
        WiFi.disconnect();
        delay(100);
    }
    
    // Step 4: Set mode and start connection
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.ssid.c_str(), config.password.c_str());
    _lastConnectAttempt = millis();
    
    // Step 5: Wait for connection with timeout
    unsigned long startTime = millis();
    while (!WiFi.isConnected() && (millis() - startTime) < config.connectTimeout) {
        delay(500);
        Serial.print(".");
        yield();  // Allow background tasks
    }
    Serial.println();
    
    // Step 6: Check connection result
    if (WiFi.isConnected()) {
        setState(WifiMgrState::CONNECTED);
        _reconnectCount = 0;
        
        Serial.println(F("[WiFiManager] Connected!"));
        Serial.printf("  IP: %s\n", getIP().c_str());
        Serial.printf("  RSSI: %d dBm (%d%%)\n", getRSSI(), getSignalQuality());
        
        // Step 7: Setup mDNS
        setupMDNS();
        
        return ErrorCode::ERR_NONE;
    }
    
    // Step 8: Connection failed
    setState(WifiMgrState::CONNECTION_FAILED);
    _reconnectCount++;
    
    return ErrorHandler::getInstance().logError(ErrorCode::ERR_WIFI_CONNECT,
        "SSID: " + config.ssid);
}

/**
 * @brief Disconnect from current network
 */
void WiFiManager::disconnect() {
    Serial.println(F("[WiFiManager] Disconnecting..."));
    WiFi.disconnect();
    setState(WifiMgrState::DISCONNECTED);
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
    
    Serial.printf("[WiFiManager] Starting AP: %s\n", apSSID.c_str());
    Serial.printf("[WiFiManager] AP IP: %s\n", AP_IP.toString().c_str());
    
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
    
    Serial.println(F("[WiFiManager] AP Started"));
    Serial.printf("  Connect to WiFi: %s\n", apSSID.c_str());
    if (config.apPassword.length() >= 8) {
        Serial.printf("  Password: %s\n", config.apPassword.c_str());
    } else {
        Serial.println(F("  Password: (open network)"));
    }
    Serial.printf("  Then open: http://%s\n", AP_IP.toString().c_str());
    
    // Step 5: Setup mDNS for AP mode
    setupMDNS();
    
    return ErrorCode::ERR_NONE;
}

/**
 * @brief Stop Access Point mode
 */
void WiFiManager::stopAP() {
    Serial.println(F("[WiFiManager] Stopping AP..."));
    
    if (_dnsServer) {
        delete _dnsServer;
        _dnsServer = nullptr;
    }
    
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
}

/**
 * @brief Setup DNS server for captive portal
 */
void WiFiManager::setupCaptivePortal() {
    if (_dnsServer) {
        delete _dnsServer;
    }
    
    _dnsServer = new DNSServer();
    
    // Redirect all DNS queries to AP IP (captive portal)
    _dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    _dnsServer->start(DNS_PORT, "*", AP_IP);
    
    Serial.println(F("[WiFiManager] Captive portal DNS started"));
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
        Serial.printf("[OTA] Start updating %s — freeing heap...\n", type);
        if (_otaPrepareCallback) {
            _otaPrepareCallback();
        }
        Serial.printf("[OTA] Free heap before transfer: %u bytes\n", (unsigned)ESP.getFreeHeap());
    });
    
    ArduinoOTA.onEnd([this]() {
        _otaInProgress = false;
        Serial.println(F("\n[OTA] Update complete!"));
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static int lastPercent = -1;
        int percent = (progress / (total / 100));
        if (percent != lastPercent && percent % 10 == 0) {
            Serial.printf("[OTA] Progress: %u%%\n", percent);
            lastPercent = percent;
        }
    });
    
    ArduinoOTA.onError([this](ota_error_t error) {
        _otaInProgress = false;
        Serial.printf("[OTA] Error[%u]: ", error);
        switch (error) {
            case OTA_AUTH_ERROR:    Serial.println(F("Auth Failed")); break;
            case OTA_BEGIN_ERROR:   Serial.println(F("Begin Failed")); break;
            case OTA_CONNECT_ERROR: Serial.println(F("Connect Failed")); break;
            case OTA_RECEIVE_ERROR: Serial.println(F("Receive Failed")); break;
            case OTA_END_ERROR:     Serial.println(F("End Failed")); break;
        }
        if (_otaErrorCallback) {
            _otaErrorCallback();
        }
    });
    
    // Step 4: Start OTA
    ArduinoOTA.begin();
    Serial.println(F("[WiFiManager] OTA initialized"));
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
 * @brief Check and handle reconnection
 */
void WiFiManager::checkConnection() {
    // Skip if in AP mode or connecting
    if (_state == WifiMgrState::AP_MODE || _state == WifiMgrState::CONNECTING) {
        return;
    }
    
    // Check if connection lost
    if (_state == WifiMgrState::CONNECTED && !WiFi.isConnected()) {
        Serial.println(F("[WiFiManager] Connection lost"));
        setState(WifiMgrState::DISCONNECTED);
        ErrorHandler::getInstance().logError(ErrorCode::ERR_WIFI_DISCONNECTED);
    }
    
    // Attempt reconnection
    if (_state == WifiMgrState::DISCONNECTED || _state == WifiMgrState::CONNECTION_FAILED) {
        if (millis() - _lastConnectAttempt > _reconnectInterval) {
            if (_reconnectCount < MAX_RECONNECT_ATTEMPTS) {
                Serial.println(F("[WiFiManager] Attempting reconnection..."));
                connect();
            } else {
                // Max reconnection attempts reached, switch to AP mode
                Serial.println(F("[WiFiManager] Max reconnect attempts, switching to AP"));
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
        Serial.printf("[WiFiManager] mDNS: http://%s.local\n", config.hostname.c_str());
    } else {
        Serial.println(F("[WiFiManager] mDNS setup failed"));
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
    Serial.println(F("[WiFiManager] Scanning networks..."));
    
    int numNetworks = WiFi.scanNetworks();
    const int maxNet = 10;
    int n = (numNetworks > maxNet) ? maxNet : numNetworks;

    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();

    for (int i = 0; i < n; i++) {
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
