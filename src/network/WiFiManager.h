/**
 * @file WiFiManager.h
 * @brief WiFi connection management for ESP8266 with OTA support
 * 
 * This module provides:
 * - Station mode connection with auto-reconnect
 * - Access Point mode for configuration (FluidLM_<deviceid>)
 * - Captive portal for easy configuration
 * - OTA (Over-The-Air) update support
 * - Connection status monitoring
 * - mDNS support for easy discovery
 *
 * @note Call `loop()` from main. OTA prepare callback may tear down HTTP/MQTT/WS.
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <functional>
#include "../config/ConfigManager.h"
#include "../utils/ErrorHandler.h"

// =============================================================================
// SECTION 1: WIFI STATE ENUM
// =============================================================================

/**
 * @enum WifiMgrState
 * @brief WiFi connection states (renamed to avoid ESP8266 WiFiState conflict)
 */
enum class WifiMgrState {
    DISCONNECTED,       ///< Not connected
    CONNECTING,         ///< Connection in progress
    CONNECTED,          ///< Connected to AP
    AP_MODE,            ///< Running as Access Point
    CONNECTION_FAILED   ///< Connection failed
};

// =============================================================================
// SECTION 2: CALLBACK TYPES
// =============================================================================

/**
 * @typedef WiFiCallback
 * @brief Callback for WiFi state changes
 */
typedef std::function<void(WifiMgrState state)> WiFiCallback;
typedef std::function<void()> OTAPrepareCallback;
typedef std::function<void()> OTAErrorCallback;

// =============================================================================
// SECTION 3: CONSTANTS
// =============================================================================

#define DNS_PORT            53
#define AP_IP               IPAddress(192, 168, 4, 1)
#define AP_GATEWAY          IPAddress(192, 168, 4, 1)
#define AP_SUBNET           IPAddress(255, 255, 255, 0)

// =============================================================================
// SECTION 4: WIFI MANAGER CLASS
// =============================================================================

/**
 * @class WiFiManager
 * @brief Manages WiFi connectivity with OTA support
 * 
 * Features:
 * - Automatic connection and reconnection
 * - Fallback to AP mode if connection fails
 * - Dynamic AP SSID: FluidLM_<chipid>
 * - Captive portal for configuration
 * - OTA updates
 * - mDNS support
 */
class WiFiManager {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to WiFiManager instance
     */
    static WiFiManager& getInstance();
    
    /**
     * @brief Initialize WiFi manager
     * @return ErrorCode indicating success or failure
     */
    ErrorCode begin();
    
    /**
     * @brief Process WiFi tasks (call in loop)
     */
    void loop();
    
    /**
     * @brief Connect to configured WiFi network
     * @return ErrorCode indicating success or failure
     */
    ErrorCode connect();
    
    /**
     * @brief Disconnect from current network
     */
    void disconnect();
    
    /**
     * @brief Start Access Point mode with captive portal
     * @return ErrorCode indicating success or failure
     */
    ErrorCode startAP();
    
    /**
     * @brief Stop Access Point mode
     */
    void stopAP();
    
    /**
     * @brief Get current WiFi state
     * @return Current WifiMgrState
     */
    WifiMgrState getState() const { return _state; }
    
    /**
     * @brief Check if connected to WiFi
     * @return true if connected
     */
    bool isConnected() const { return _state == WifiMgrState::CONNECTED; }
    
    /**
     * @brief Check if in AP mode
     * @return true if AP mode active
     */
    bool isAPMode() const { return _state == WifiMgrState::AP_MODE; }
    
    /**
     * @brief Get IP address
     * @return IP address string
     */
    String getIP() const;
    
    /**
     * @brief Get AP IP address
     * @return AP IP address string
     */
    String getAPIP() const;
    
    /**
     * @brief Get MAC address
     * @return MAC address string
     */
    String getMAC() const;
    
    /**
     * @brief Get device ID (chip ID in hex)
     * @return Device ID string
     */
    String getDeviceId() const;
    
    /**
     * @brief Get dynamic AP SSID (FluidLM_<chipid>)
     * @return AP SSID string
     */
    String getAPSSID() const;
    
    /**
     * @brief Get WiFi signal strength (RSSI)
     * @return RSSI value in dBm
     */
    int getRSSI() const;
    
    /**
     * @brief Get signal quality percentage
     * @return Signal quality 0-100%
     */
    int getSignalQuality() const;
    
    /**
     * @brief Get connected SSID
     * @return SSID string
     */
    String getSSID() const;
    
    /**
     * @brief Scan for available networks
     * @return JSON array of networks
     */
    String scanNetworks();
    
    /**
     * @brief Set state change callback
     * @param callback Function to call on state change
     */
    void setStateCallback(WiFiCallback callback);

    /** Called before ArduinoOTA starts — free heap (MQTT, WS, web). */
    void setOTAPrepareCallback(OTAPrepareCallback cb) { _otaPrepareCallback = cb; }

    /** Called on ArduinoOTA error — restore services if OTA aborted. */
    void setOTAErrorCallback(OTAErrorCallback cb) { _otaErrorCallback = cb; }
    
    /**
     * @brief Get WiFi status as JSON
     * @return JSON string with WiFi status
     */
    String getStatusJson() const;
    
    /**
     * @brief Convert state to string
     * @param state WifiMgrState value
     * @return String representation
     */
    static String stateToString(WifiMgrState state);
    
    /**
     * @brief Check if OTA is in progress
     * @return true if OTA update running
     */
    bool isOTAInProgress() const { return _otaInProgress; }

private:
    // Private constructor for singleton
    WiFiManager();
    
    // Delete copy constructor and assignment
    WiFiManager(const WiFiManager&) = delete;
    WiFiManager& operator=(const WiFiManager&) = delete;
    
    // Update state and call callback
    void setState(WifiMgrState newState);
    
    // Check and handle reconnection
    void checkConnection();
    
    // Setup mDNS
    void setupMDNS();
    
    // Setup OTA
    void setupOTA();
    
    // Setup DNS for captive portal
    void setupCaptivePortal();
    
    // Member variables
    bool _initialized;              ///< Initialization flag
    WifiMgrState _state;            ///< Current state
    WiFiCallback _stateCallback;    ///< State change callback
    OTAPrepareCallback _otaPrepareCallback;
    OTAErrorCallback _otaErrorCallback;
    unsigned long _lastConnectAttempt;  ///< Last connection attempt time
    unsigned long _reconnectInterval;   ///< Reconnection interval
    uint8_t _reconnectCount;        ///< Reconnection attempt count
    bool _otaInProgress;            ///< OTA update in progress
    DNSServer* _dnsServer;          ///< DNS server for captive portal
    
    static const uint8_t MAX_RECONNECT_ATTEMPTS = 3;
};

#endif // WIFI_MANAGER_H
