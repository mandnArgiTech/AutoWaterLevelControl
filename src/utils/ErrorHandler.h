/**
 * @file ErrorHandler.h
 * @brief Comprehensive error handling system for FluidLevelMonitor
 * 
 * This module provides centralized error handling with:
 * - Error code definitions
 * - Error logging
 * - Error state tracking
 * - JSON error descriptions loaded from LittleFS
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <functional>

// =============================================================================
// SECTION 1: ERROR CODE DEFINITIONS
// =============================================================================

/**
 * @enum ErrorCode
 * @brief Comprehensive error codes for all system components
 * 
 * Error codes are organized by category:
 * - 0xx: System errors
 * - 1xx: Sensor errors
 * - 2xx: Tank calculation errors
 * - 3xx: WiFi errors
 * - 4xx: MQTT errors
 * - 5xx: Configuration/Storage errors
 * - 6xx: Web server errors
 * - 7xx: Time errors
 */
enum class ErrorCode : uint16_t {
    // System errors (0xx)
    ERR_NONE                = 0,    ///< No error
    ERR_SYSTEM_INIT         = 1,    ///< System initialization failed
    ERR_SYSTEM_MEMORY       = 2,    ///< Insufficient memory
    ERR_SYSTEM_WATCHDOG     = 3,    ///< Watchdog triggered
    
    // Sensor errors (1xx)
    ERR_SENSOR_INIT         = 100,  ///< Sensor initialization failed
    ERR_SENSOR_TIMEOUT      = 101,  ///< Sensor timeout
    ERR_SENSOR_INVALID_DATA = 102,  ///< Invalid sensor data
    ERR_SENSOR_DISTANCE_MIN = 103,  ///< Distance below minimum
    ERR_SENSOR_DISTANCE_MAX = 104,  ///< Distance above maximum
    ERR_SENSOR_TEMP_INVALID = 105,  ///< Invalid temperature
    
    // Tank errors (2xx)
    ERR_TANK_CONFIG         = 200,  ///< Invalid tank config
    ERR_TANK_LEVEL_NEGATIVE = 201,  ///< Negative water level
    ERR_TANK_LEVEL_OVERFLOW = 202,  ///< Water level > 100%
    ERR_TANK_EMPTY          = 203,  ///< Tank empty
    ERR_TANK_FULL           = 204,  ///< Tank full
    
    // WiFi errors (3xx)
    ERR_WIFI_INIT           = 300,  ///< WiFi init failed
    ERR_WIFI_CONNECT        = 301,  ///< WiFi connection failed
    ERR_WIFI_DISCONNECTED   = 302,  ///< WiFi disconnected
    ERR_WIFI_NO_SSID        = 303,  ///< No SSID configured
    ERR_WIFI_AP_FAILED      = 304,  ///< AP mode failed
    
    // MQTT errors (4xx)
    ERR_MQTT_INIT           = 400,  ///< MQTT init failed
    ERR_MQTT_CONNECT        = 401,  ///< MQTT connection failed
    ERR_MQTT_DISCONNECTED   = 402,  ///< MQTT disconnected
    ERR_MQTT_PUBLISH        = 403,  ///< MQTT publish failed
    ERR_MQTT_SUBSCRIBE      = 404,  ///< MQTT subscribe failed
    ERR_MQTT_DISABLED       = 405,  ///< MQTT disabled
    
    // Config errors (5xx)
    ERR_CONFIG_INIT         = 500,  ///< Config init failed
    ERR_CONFIG_READ         = 501,  ///< Config read failed
    ERR_CONFIG_WRITE        = 502,  ///< Config write failed
    ERR_CONFIG_PARSE        = 503,  ///< Config parse failed
    ERR_CONFIG_VALIDATE     = 504,  ///< Config validation failed
    ERR_FS_MOUNT            = 505,  ///< Filesystem mount failed
    ERR_FS_FULL             = 506,  ///< Filesystem full
    ERR_CONFIG_LOCKED       = 507,  ///< Config write already in progress
    
    // Web errors (6xx)
    ERR_WEB_INIT            = 600,  ///< Web server init failed
    ERR_WEB_REQUEST         = 601,  ///< Invalid request
    ERR_WEB_AUTH            = 602,  ///< Authentication failed
    ERR_WEB_FILE_NOT_FOUND  = 603,  ///< File not found
    
    // Time errors (7xx)
    ERR_TIME_INIT           = 700,  ///< Time init failed
    ERR_TIME_SYNC           = 701,  ///< Time sync failed
    ERR_TIME_INVALID        = 702   ///< Time not synchronized
};

// =============================================================================
// SECTION 2: ERROR SEVERITY LEVELS
// =============================================================================

/**
 * @enum ErrorSeverity
 * @brief Severity levels for error classification
 */
enum class ErrorSeverity : uint8_t {
    INFO     = 0,   ///< Informational message
    WARNING  = 1,   ///< Warning - operation continues
    ERROR    = 2,   ///< Error - operation failed
    CRITICAL = 3    ///< Critical - system needs attention
};

// =============================================================================
// SECTION 3: ERROR ENTRY STRUCTURE
// =============================================================================

/**
 * @struct ErrorEntry
 * @brief Structure to hold error information
 */
struct ErrorEntry {
    ErrorCode code;                 ///< Error code
    ErrorSeverity severity;         ///< Error severity
    String name;                    ///< Error name
    String description;             ///< Error description
    String action;                  ///< Recommended action
    unsigned long timestamp;        ///< When error occurred
    uint32_t count;                 ///< Occurrence count
};

// =============================================================================
// SECTION 4: ERROR CALLBACK TYPE
// =============================================================================

/**
 * @typedef ErrorCallback
 * @brief Callback function type for error notifications
 */
typedef std::function<void(ErrorCode code, ErrorSeverity severity, const String& message)> ErrorCallback;

// =============================================================================
// SECTION 5: ERROR HANDLER CLASS
// =============================================================================

/**
 * @class ErrorHandler
 * @brief Singleton class for centralized error handling
 * 
 * This class provides:
 * - Error logging with timestamps
 * - Error descriptions from JSON
 * - Error history tracking
 * - Error callbacks for notifications
 */
class ErrorHandler {
public:
    // Maximum errors to keep in history
    static const size_t MAX_ERROR_HISTORY = 20;
    
    /**
     * @brief Get singleton instance
     * @return Reference to ErrorHandler instance
     */
    static ErrorHandler& getInstance();
    
    /**
     * @brief Initialize error handler and load error descriptions
     * @return true if initialization successful
     */
    bool begin();
    
    /**
     * @brief Log an error
     * @param code Error code to log
     * @param additionalInfo Optional additional context
     * @return The error code for chaining
     */
    ErrorCode logError(ErrorCode code, const String& additionalInfo = "");
    
    /**
     * @brief Get the last error code
     * @return Last logged error code
     */
    ErrorCode getLastError() const;
    
    /**
     * @brief Get last error entry with full details
     * @return Pointer to last error entry or nullptr
     */
    const ErrorEntry* getLastErrorEntry() const;
    
    /**
     * @brief Clear all errors
     */
    void clearErrors();
    
    /**
     * @brief Check if any errors are active
     * @return true if there are errors
     */
    bool hasErrors() const;
    
    /**
     * @brief Get error count
     * @return Number of errors in history
     */
    size_t getErrorCount() const;
    
    /**
     * @brief Get error description from code
     * @param code Error code
     * @return Error description string
     */
    String getErrorDescription(ErrorCode code) const;
    
    /**
     * @brief Get error name from code
     * @param code Error code
     * @return Error name string
     */
    String getErrorName(ErrorCode code) const;
    
    /**
     * @brief Get severity level for error code
     * @param code Error code
     * @return Severity level
     */
    ErrorSeverity getErrorSeverity(ErrorCode code) const;
    
    /**
     * @brief Get severity as string
     * @param severity Severity level
     * @return Severity string
     */
    static String severityToString(ErrorSeverity severity);
    
    /**
     * @brief Set callback for error notifications
     * @param callback Function to call on errors
     */
    void setErrorCallback(ErrorCallback callback);
    
    /**
     * @brief Get error history as JSON
     * @return JSON string of error history
     */
    String getErrorHistoryJson() const;
    
    /**
     * @brief Get specific error info as JSON
     * @param code Error code
     * @return JSON string of error info
     */
    String getErrorInfoJson(ErrorCode code) const;
    
    /**
     * @brief Print error to Serial
     * @param code Error code
     * @param additionalInfo Optional additional info
     */
    void printError(ErrorCode code, const String& additionalInfo = "") const;

private:
    // Private constructor for singleton
    ErrorHandler();
    
    // Delete copy constructor and assignment
    ErrorHandler(const ErrorHandler&) = delete;
    ErrorHandler& operator=(const ErrorHandler&) = delete;
    
    void lookupError(ErrorCode code, String& name, String& desc, ErrorSeverity& sev) const;
    void lookupErrorFromFile(ErrorCode code, String& name, String& desc, ErrorSeverity& sev) const;
    static ErrorSeverity defaultSeverityFromCode(uint16_t codeNum);

    static const size_t LOOKUP_CACHE_SIZE = 8;
    mutable struct {
        ErrorCode code;
        bool valid;
        String name;
        String desc;
        ErrorSeverity sev;
    } _lookupCache[LOOKUP_CACHE_SIZE];
    mutable uint8_t _lookupCacheNext;

    // Member variables
    bool _initialized;                              ///< Initialization flag
    ErrorEntry _errorHistory[MAX_ERROR_HISTORY];    ///< Error history buffer
    size_t _errorIndex;                             ///< Current index in history
    size_t _errorCount;                             ///< Total errors logged
    ErrorCode _lastError;                           ///< Last error code
    ErrorCallback _errorCallback;                   ///< Error callback function
    bool _errorsFilePresent;                        ///< /errors.json exists (no RAM cache)
    static const unsigned long PRINT_INTERVAL_MS = 10000;
    ErrorCode _lastPrintedCode;
    unsigned long _lastPrintTime;
    uint32_t _suppressedCount;
};

// =============================================================================
// SECTION 6: CONVENIENCE MACROS
// =============================================================================

/**
 * @brief Log error with file and line info (debug builds)
 */
#ifdef DEBUG
    #define LOG_ERROR(code) \
        ErrorHandler::getInstance().logError(code, String(__FILE__) + ":" + String(__LINE__))
    #define LOG_ERROR_MSG(code, msg) \
        ErrorHandler::getInstance().logError(code, String(__FILE__) + ":" + String(__LINE__) + " - " + msg)
#else
    #define LOG_ERROR(code) \
        ErrorHandler::getInstance().logError(code)
    #define LOG_ERROR_MSG(code, msg) \
        ErrorHandler::getInstance().logError(code, msg)
#endif

/**
 * @brief Check and return if error
 */
#define CHECK_ERROR(code) \
    if ((code) != ErrorCode::ERR_NONE) { \
        LOG_ERROR(code); \
        return code; \
    }

#endif // ERROR_HANDLER_H

