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
#include "../motor/IMotorController.h"

// =============================================================================
// SECTION 1: CONFIGURATION FILE PATHS
// =============================================================================

#define CONFIG_FILE         "/config.json"
#define CONFIG_BACKUP_FILE  "/config.bak"
#define CONFIG_TEMP_FILE    "/config.json.new"

/** Max JSON body size for POST /api/config (bytes). */
#define FLM_MAX_CONFIG_JSON_BYTES 12288

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
#define DEFAULT_MQTT_USER           "devAdmin"
#define DEFAULT_MQTT_PASSWORD       "123456"
#define DEFAULT_MQTT_CLIENT_ID      "FluidMonitor"
#define DEFAULT_MQTT_DEVICE_NAME    "tank1"
#define DEFAULT_MQTT_TOPIC_PREFIX   "water"
#define DEFAULT_MQTT_PUBLISH_INTERVAL 60000  // 60 seconds
#define DEFAULT_MQTT_TLS            false
#define DEFAULT_MQTT_TLS_MODE       "insecure"  // insecure | fingerprint | ca
#define DEFAULT_MQTT_FINGERPRINT    ""

// Tank defaults (Sintex tank dimensions in mm)
#define DEFAULT_TANK_TYPE           "circular"
#define DEFAULT_TANK_DIAMETER       1350.0f  // mm
#define DEFAULT_TANK_HEIGHT         1704.5f  // mm
#define DEFAULT_TANK_VOLUME_LITERS  2438.0f  // Calculated: π * (675)² * 1704.5 / 1000000

// Sensor defaults
#define DEFAULT_SENSOR_TYPE         "A02YYUW"  // DYP-A02YYUW default level sensor
#define DEFAULT_SENSOR_OFFSET       50.0f    // Distance from sensor to full tank (mm)
#define DEFAULT_SENSOR_MIN_DISTANCE 280.0f   // A02YYUW blind zone (mm)
#define DEFAULT_SENSOR_MAX_DISTANCE 7500.0f  // A02YYUW max range (mm)
#define DEFAULT_SENSOR_SAMPLES      5        // Number of samples for averaging
#define DEFAULT_SENSOR_INTERVAL     2000     // ms between readings

// Sensor hardware defaults (NodeMCU pins)
#define DEFAULT_RX_PIN              14       // D5 / GPIO14 - for UART sensors (sensor TX -> ESP RX)
#define DEFAULT_TX_PIN              12       // D6 / GPIO12 - for UART sensors (sensor RX <- ESP TX)
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

// Ambient (on-board DHT11 temperature + humidity)
#define DEFAULT_AMBIENT_ENABLED     true
#define DEFAULT_AMBIENT_TYPE        "DHT11"
#define DEFAULT_DHT11_PIN           2        ///< D4 / GPIO2
#define DEFAULT_DHT11_READ_INTERVAL 3000u    ///< ms between DHT11 reads

// System defaults
#define DEFAULT_DEVICE_NAME         "Water Tank Monitor"
#define DEFAULT_TIMEZONE_OFFSET     19800    // UTC+5:30 (India) in seconds
#define DEFAULT_NTP_SERVER          "pool.ntp.org"
#define DEFAULT_WEB_PORT            80
#define DEFAULT_DEBUG_ENABLED       true

// Battery A0 calibration (D1 Mini — proven ADCA1115Calibration path)
#define DEFAULT_BATTERY_CAL_OFFSET  0.0f
#ifndef DEFAULT_BATTERY_CELLS
#define DEFAULT_BATTERY_CELLS       1
#endif
#define DEFAULT_BATTERY_AUTO_CAL    false

// Motor / relay / SMS (motor_* firmware; ignored on sensor_only)
#define DEFAULT_RELAY_ENABLED           false
#define DEFAULT_RELAY_PIN               12       ///< D6 — avoid GPIO14 (common XKC signal pin)
#define DEFAULT_RELAY_ACTIVE_LOW        true
#define DEFAULT_RELAY_PUMP_ON_PCT       20.0f
#define DEFAULT_RELAY_PUMP_OFF_PCT      85.0f
#define DEFAULT_RELAY_MAX_RUN_MIN       30
#define DEFAULT_GSM_RX_PIN              5        ///< D1
#define DEFAULT_GSM_TX_PIN              4        ///< D2
#define DEFAULT_GSM_BAUD                9600
#define DEFAULT_SMS_CONFIRM_MS          30000u

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
    uint16_t port;              ///< Broker port (8883 typical for TLS)
    String username;            ///< Authentication username
    String password;            ///< Authentication password
    String clientId;            ///< Client identifier
    String deviceName;          ///< Short device name for topic (e.g. "kitchen_tank")
    String topicPrefix;         ///< Base topic prefix
    uint32_t publishInterval;   ///< Interval between publishes (ms)
    bool tls;                   ///< Use TLS (BearSSL WiFiClientSecure)
    String tlsMode;             ///< "insecure" (encrypt only), "fingerprint", or "ca"
    String fingerprint;         ///< SHA-1 cert fingerprint (tlsMode=fingerprint)
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
    String type;                ///< Sensor type: US100, A02YYUW, HC_SR04, TF_LUNA, XKC_KD200
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
 * @struct AmbientConfig
 * @brief On-board ambient temperature/humidity (DHT11)
 */
struct AmbientConfig {
    bool enabled;
    String type;
    int pin;
    uint32_t readIntervalMs;
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

    AmbientConfig ambient;      ///< DHT11 (or future ambient sensors)
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

/**
 * @struct BatteryConfig
 * @brief D1 Mini A0 battery monitor calibration
 */
struct BatteryConfig {
    float calibrationOffset;    ///< actual − measured (Volts)
    uint8_t cellsInSeries;      ///< 1 = single cell, 2 = 2S pack
    bool autoCalibration;       ///< true after a successful cal
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
     * @brief Begin exclusive config update (REST full POST). Pair with unlockConfigWrite().
     * @return false if another transaction is active
     */
    bool tryLockForConfigWrite();

    /** End exclusive config transaction. */
    void unlockConfigWrite();

    /**
     * @brief Save current configuration to LittleFS (atomic replace). If a transaction lock
     *        is held, save runs under that lock; otherwise acquires lock for this call only.
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
    BatteryConfig& getBatteryConfig() { return _batteryConfig; }
    
    // Const accessors
    const WiFiConfig& getWiFiConfig() const { return _wifiConfig; }
    const MQTTConfig& getMQTTConfig() const { return _mqttConfig; }
    const TankConfig& getTankConfig() const { return _tankConfig; }
    const SensorConfig& getSensorConfig() const { return _sensorConfig; }
    const SystemConfig& getSystemConfig() const { return _systemConfig; }
    const BatteryConfig& getBatteryConfig() const { return _batteryConfig; }
    MotorConfig& getMotorConfig() { return _motorConfig; }
    const MotorConfig& getMotorConfig() const { return _motorConfig; }
    
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
     * @param section Section name ("wifi", "mqtt", "tank", "sensor", "system", "relay", "battery")
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
    void setDefaultBatteryConfig();
    void setDefaultMotorConfig();
    
    // Parse JSON sections
    void parseWiFiConfig(const JsonObject& obj);
    void parseMQTTConfig(const JsonObject& obj);
    void parseTankConfig(const JsonObject& obj);
    void parseSensorConfig(const JsonObject& obj);
    void parseSystemConfig(const JsonObject& obj);
    void parseBatteryConfig(const JsonObject& obj);
    void parseMotorConfig(const JsonObject& obj);
    
    // Serialize to JSON
    void serializeWiFiConfig(JsonObject& obj) const;
    void serializeMQTTConfig(JsonObject& obj) const;
    void serializeTankConfig(JsonObject& obj) const;
    void serializeSensorConfig(JsonObject& obj) const;
    void serializeSystemConfig(JsonObject& obj) const;
    void serializeBatteryConfig(JsonObject& obj) const;
    void serializeMotorConfig(JsonObject& obj) const;

    ErrorCode saveConfigAtomic();
    void applyNumericClamps();
    
    // Member variables
    bool _initialized;          ///< Initialization flag
    bool _fsMounted;            ///< Filesystem mounted flag
    
    WiFiConfig _wifiConfig;     ///< WiFi configuration
    MQTTConfig _mqttConfig;     ///< MQTT configuration
    TankConfig _tankConfig;     ///< Tank configuration
    SensorConfig _sensorConfig; ///< Sensor configuration
    SystemConfig _systemConfig; ///< System configuration
    BatteryConfig _batteryConfig; ///< A0 battery calibration
    MotorConfig  _motorConfig;   ///< Relay + SMS motor section (`relay` in JSON)

    volatile bool _configWriteLocked;  ///< Serialize HTTP full config POST vs other saves
};

#endif // CONFIG_MANAGER_H

