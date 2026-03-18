/**
 * @file ConfigManager.h
 * @brief Configuration management system using LittleFS
 * 
 * This module handles all configuration storage and retrieval:
 * - WiFi settings
 * - MQTT settings
 * - Tank dimensions
 * - Sensor calibration
 * - System settings
 * 
 * All configurations are stored in JSON format on LittleFS.
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "../utils/ErrorHandler.h"

// =============================================================================
// SECTION 1: CONFIGURATION FILE PATHS
// =============================================================================

#define CONFIG_FILE         "/config.json"
#define CONFIG_BACKUP_FILE  "/config.bak"

// =============================================================================
// SECTION 2: DEFAULT VALUES
// =============================================================================

// WiFi defaults
#define DEFAULT_WIFI_SSID           ""
#define DEFAULT_WIFI_PASSWORD       ""
#define DEFAULT_WIFI_HOSTNAME       "FluidMonitor"
#define DEFAULT_WIFI_AP_SSID        "FluidMonitor-AP"
#define DEFAULT_WIFI_AP_PASSWORD    "12345678"
#define DEFAULT_WIFI_CONNECT_TIMEOUT 30000  // 30 seconds

// MQTT defaults
#define DEFAULT_MQTT_ENABLED        false
#define DEFAULT_MQTT_SERVER         ""
#define DEFAULT_MQTT_PORT           1883
#define DEFAULT_MQTT_USER           ""
#define DEFAULT_MQTT_PASSWORD       ""
#define DEFAULT_MQTT_CLIENT_ID      "FluidMonitor"
#define DEFAULT_MQTT_DEVICE_NAME    "tank1"
#define DEFAULT_MQTT_TOPIC_PREFIX   "water"
#define DEFAULT_MQTT_PUBLISH_INTERVAL 60000  // 60 seconds

// Tank defaults (Sintex tank dimensions in mm)
#define DEFAULT_TANK_TYPE           "circular"
#define DEFAULT_TANK_DIAMETER       1350.0f  // mm
#define DEFAULT_TANK_HEIGHT         1704.5f  // mm
#define DEFAULT_TANK_VOLUME_LITERS  2438.0f  // Calculated: π * (675)² * 1704.5 / 1000000

// Sensor defaults
#define DEFAULT_SENSOR_TYPE         "US100"  // Default sensor type
#define DEFAULT_SENSOR_OFFSET       50.0f    // Distance from sensor to full tank (mm)
#define DEFAULT_SENSOR_MIN_DISTANCE 20.0f    // Minimum range (mm)
#define DEFAULT_SENSOR_MAX_DISTANCE 4500.0f  // Maximum range (mm)
#define DEFAULT_SENSOR_SAMPLES      5        // Number of samples for averaging
#define DEFAULT_SENSOR_INTERVAL     2000     // ms between readings

// Sensor hardware defaults (NodeMCU pins)
#define DEFAULT_RX_PIN              5        // D1 / GPIO5 - for UART sensors
#define DEFAULT_TX_PIN              4        // D2 / GPIO4 - for UART sensors
#define DEFAULT_TRIG_PIN            5        // D1 / GPIO5 - for HC-SR04
#define DEFAULT_ECHO_PIN            4        // D2 / GPIO4 - for HC-SR04
#define DEFAULT_SIGNAL_PIN          14       // D5 / GPIO14 - for XKC-KD200
#define DEFAULT_MOUNT_HEIGHT_MM     500.0f   // Default mount height for point sensors
#define DEFAULT_INVERTED_LOGIC      false    // Normal logic (HIGH = detected)

// Filter defaults for fluid surface stability
#define DEFAULT_FILTER_ENABLED      true     // Enable filtering by default
#define DEFAULT_MEDIAN_FILTER_SIZE  5        // Median filter removes spikes
#define DEFAULT_MOVING_AVG_WINDOW   10       // Moving average smooths ripples

// Kalman filter defaults
#define DEFAULT_KALMAN_ENABLED      true     // Enable Kalman filter by default
#define DEFAULT_KALMAN_PROCESS_Q    0.01f    // Process noise (lower = smoother)
#define DEFAULT_KALMAN_MEASURE_R    0.1f     // Measurement noise (higher = more filtering)

// System defaults
#define DEFAULT_DEVICE_NAME         "Water Tank Monitor"
#define DEFAULT_TIMEZONE_OFFSET     19800    // UTC+5:30 (India) in seconds
#define DEFAULT_NTP_SERVER          "pool.ntp.org"
#define DEFAULT_WEB_PORT            80
#define DEFAULT_DEBUG_ENABLED       true

// =============================================================================
// SECTION 3: CONFIGURATION STRUCTURES
// =============================================================================

/**
 * @struct WiFiConfig
 * @brief WiFi connection settings
 */
struct WiFiConfig {
    String ssid;                ///< WiFi network name
    String password;            ///< WiFi password
    String hostname;            ///< mDNS hostname
    String apSsid;              ///< AP mode SSID
    String apPassword;          ///< AP mode password
    uint32_t connectTimeout;    ///< Connection timeout in ms
    bool apMode;                ///< Force AP mode
};

/**
 * @struct MQTTConfig
 * @brief MQTT broker settings
 */
struct MQTTConfig {
    bool enabled;               ///< MQTT enabled flag
    String server;              ///< Broker hostname/IP
    uint16_t port;              ///< Broker port
    String username;            ///< Authentication username
    String password;            ///< Authentication password
    String clientId;            ///< Client identifier
    String deviceName;          ///< Short device name for topic (e.g. "kitchen_tank")
    String topicPrefix;         ///< Base topic prefix
    uint32_t publishInterval;   ///< Interval between publishes (ms)
};

/**
 * @struct TankConfig
 * @brief Tank physical dimensions
 */
struct TankConfig {
    String type;                ///< Tank type: "circular" or "rectangular"
    float diameter;             ///< Diameter for circular tank (mm)
    float length;               ///< Length for rectangular tank (mm)
    float width;                ///< Width for rectangular tank (mm)
    float height;               ///< Tank height (mm)
    float volumeLiters;         ///< Total volume in liters (calculated or manual)
};

/**
 * @struct SensorHWConfig
 * @brief Sensor hardware/pin configuration
 */
struct SensorHWConfig {
    String type;                ///< Sensor type: US100, HC_SR04, TF_LUNA, XKC_KD200
    int rxPin;                  ///< RX pin (for UART sensors: US100, TF-Luna)
    int txPin;                  ///< TX pin (for UART sensors: US100, TF-Luna)
    int trigPin;                ///< Trigger pin (for HC-SR04)
    int echoPin;                ///< Echo pin (for HC-SR04)
    int signalPin;              ///< Signal pin (for XKC-KD200)
    float mountHeightMm;        ///< Mount height for point sensors (XKC-KD200)
    float tankHeightMm;         ///< Tank height for point sensors
    bool invertedLogic;         ///< Inverted logic for XKC-KD200
};

/**
 * @struct SensorConfig
 * @brief Sensor configuration and calibration settings
 */
struct SensorConfig {
    // Hardware configuration
    SensorHWConfig hardware;    ///< Hardware/pin configuration
    
    // Calibration
    float offsetMm;             ///< Distance from sensor to full tank level (mm)
    float minDistance;          ///< Minimum valid distance (mm)
    float maxDistance;          ///< Maximum valid distance (mm)
    uint8_t samples;            ///< Number of samples for averaging
    uint32_t readInterval;      ///< Interval between readings (ms)
    
    // Filter settings for fluid surface stability
    bool filterEnabled;         ///< Enable combined filtering
    uint8_t medianFilterSize;   ///< Median filter size (removes spikes)
    uint8_t movingAvgWindow;    ///< Moving average window (smooths ripples)
    
    // Kalman filter settings
    bool kalmanEnabled;         ///< Enable Kalman filter (optimal estimation)
    float kalmanProcessNoise;   ///< Q - Process noise (lower = smoother)
    float kalmanMeasureNoise;   ///< R - Measurement noise (higher = more filtering)
};

/**
 * @struct SystemConfig
 * @brief General system settings
 */
struct SystemConfig {
    String deviceName;          ///< User-friendly device name
    int32_t timezoneOffset;     ///< Timezone offset in seconds from UTC
    String ntpServer;           ///< NTP server address
    uint16_t webPort;           ///< Web server port
    bool debugEnabled;          ///< Debug output enabled
};

// =============================================================================
// SECTION 4: CONFIG MANAGER CLASS
// =============================================================================

/**
 * @class ConfigManager
 * @brief Singleton class for configuration management
 * 
 * This class handles:
 * - Loading configuration from LittleFS
 * - Saving configuration to LittleFS
 * - Default value initialization
 * - Configuration validation
 * - Backup and restore
 */
class ConfigManager {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to ConfigManager instance
     */
    static ConfigManager& getInstance();
    
    /**
     * @brief Initialize configuration system
     * @return ErrorCode indicating success or failure
     */
    ErrorCode begin();
    
    /**
     * @brief Load configuration from LittleFS
     * @return ErrorCode indicating success or failure
     */
    ErrorCode loadConfig();
    
    /**
     * @brief Save current configuration to LittleFS
     * @return ErrorCode indicating success or failure
     */
    ErrorCode saveConfig();
    
    /**
     * @brief Reset configuration to defaults
     */
    void resetToDefaults();
    
    /**
     * @brief Create backup of current configuration
     * @return ErrorCode indicating success or failure
     */
    ErrorCode createBackup();
    
    /**
     * @brief Restore configuration from backup
     * @return ErrorCode indicating success or failure
     */
    ErrorCode restoreBackup();
    
    /**
     * @brief Validate current configuration
     * @return ErrorCode indicating validation result
     */
    ErrorCode validateConfig();
    
    // Configuration accessors
    WiFiConfig& getWiFiConfig() { return _wifiConfig; }
    MQTTConfig& getMQTTConfig() { return _mqttConfig; }
    TankConfig& getTankConfig() { return _tankConfig; }
    SensorConfig& getSensorConfig() { return _sensorConfig; }
    SystemConfig& getSystemConfig() { return _systemConfig; }
    
    // Const accessors
    const WiFiConfig& getWiFiConfig() const { return _wifiConfig; }
    const MQTTConfig& getMQTTConfig() const { return _mqttConfig; }
    const TankConfig& getTankConfig() const { return _tankConfig; }
    const SensorConfig& getSensorConfig() const { return _sensorConfig; }
    const SystemConfig& getSystemConfig() const { return _systemConfig; }
    
    /**
     * @brief Get entire configuration as JSON string
     * @return JSON string
     */
    String getConfigJson() const;
    
    /**
     * @brief Set configuration from JSON string
     * @param json JSON configuration string
     * @return ErrorCode indicating success or failure
     */
    ErrorCode setConfigFromJson(const String& json);
    
    /**
     * @brief Update specific section from JSON
     * @param section Section name ("wifi", "mqtt", "tank", "sensor", "system")
     * @param json JSON string for that section
     * @return ErrorCode indicating success or failure
     */
    ErrorCode updateSection(const String& section, const String& json);
    
    /**
     * @brief Get specific section as JSON
     * @param section Section name
     * @return JSON string for section
     */
    String getSectionJson(const String& section) const;
    
    /**
     * @brief Check if filesystem is mounted
     * @return true if mounted
     */
    bool isFilesystemMounted() const { return _fsMounted; }
    
    /**
     * @brief Get filesystem info
     * @param totalBytes Total bytes (output)
     * @param usedBytes Used bytes (output)
     */
    void getFilesystemInfo(size_t& totalBytes, size_t& usedBytes) const;
    
    /**
     * @brief Calculate tank volume in liters
     * @return Volume in liters
     */
    float calculateTankVolume() const;

private:
    // Private constructor for singleton
    ConfigManager();
    
    // Delete copy constructor and assignment
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    // Initialize LittleFS
    bool initFilesystem();
    
    // Set default values for all configs
    void setDefaultWiFiConfig();
    void setDefaultMQTTConfig();
    void setDefaultTankConfig();
    void setDefaultSensorConfig();
    void setDefaultSystemConfig();
    
    // Parse JSON sections
    void parseWiFiConfig(const JsonObject& obj);
    void parseMQTTConfig(const JsonObject& obj);
    void parseTankConfig(const JsonObject& obj);
    void parseSensorConfig(const JsonObject& obj);
    void parseSystemConfig(const JsonObject& obj);
    
    // Serialize to JSON
    void serializeWiFiConfig(JsonObject& obj) const;
    void serializeMQTTConfig(JsonObject& obj) const;
    void serializeTankConfig(JsonObject& obj) const;
    void serializeSensorConfig(JsonObject& obj) const;
    void serializeSystemConfig(JsonObject& obj) const;
    
    // Member variables
    bool _initialized;          ///< Initialization flag
    bool _fsMounted;            ///< Filesystem mounted flag
    
    WiFiConfig _wifiConfig;     ///< WiFi configuration
    MQTTConfig _mqttConfig;     ///< MQTT configuration
    TankConfig _tankConfig;     ///< Tank configuration
    SensorConfig _sensorConfig; ///< Sensor configuration
    SystemConfig _systemConfig; ///< System configuration
};

#endif // CONFIG_MANAGER_H

