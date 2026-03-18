/**
 * @file TimeManager.cpp
 * @brief Implementation of NTP time synchronization
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#include "TimeManager.h"
#include <coredecls.h>  // For settimeofday_cb

// =============================================================================
// SECTION 1: SINGLETON INSTANCE
// =============================================================================

/**
 * @brief Get singleton instance of TimeManager
 * @return Reference to the single TimeManager instance
 */
TimeManager& TimeManager::getInstance() {
    static TimeManager instance;
    return instance;
}

// =============================================================================
// SECTION 2: CONSTRUCTOR
// =============================================================================

/**
 * @brief Private constructor - initializes member variables
 */
TimeManager::TimeManager()
    : _initialized(false)
    , _synchronized(false)
    , _lastSyncAttempt(0)
    , _lastSyncSuccess(0)
    , _timezoneOffset(0) {
}

// =============================================================================
// SECTION 3: INITIALIZATION
// =============================================================================

/**
 * @brief Initialize time manager
 * @return ErrorCode indicating success or failure
 */
ErrorCode TimeManager::begin() {
    Serial.println(F("[TimeManager] Initializing..."));
    
    // Step 1: Get timezone offset from config
    _timezoneOffset = ConfigManager::getInstance().getSystemConfig().timezoneOffset;
    
    // Step 2: Configure NTP
    configureNTP();
    
    _initialized = true;
    Serial.println(F("[TimeManager] Initialized"));
    
    // Step 3: Attempt initial sync
    syncTime();
    
    return ErrorCode::ERR_NONE;
}

/**
 * @brief Configure NTP settings
 */
void TimeManager::configureNTP() {
    SystemConfig& config = ConfigManager::getInstance().getSystemConfig();
    
    // Step 1: Configure time with timezone
    // configTime takes timezone offset and daylight saving offset (0 for no DST)
    configTime(_timezoneOffset, 0, config.ntpServer.c_str(), "time.nist.gov", "time.google.com");
    
    Serial.printf("[TimeManager] NTP Server: %s\n", config.ntpServer.c_str());
    Serial.printf("[TimeManager] Timezone offset: %d seconds\n", _timezoneOffset);
}

// =============================================================================
// SECTION 4: TIME SYNCHRONIZATION
// =============================================================================

/**
 * @brief Force NTP synchronization
 * @return ErrorCode indicating success or failure
 */
ErrorCode TimeManager::syncTime() {
    Serial.println(F("[TimeManager] Synchronizing time..."));
    _lastSyncAttempt = millis();
    
    // Step 1: Wait for time to be set
    unsigned long startTime = millis();
    time_t now = time(nullptr);
    
    while (now < 1000000000 && (millis() - startTime) < NTP_SYNC_TIMEOUT) {
        delay(100);
        now = time(nullptr);
        yield();
    }
    
    // Step 2: Check if sync was successful
    if (now > 1000000000) {
        _synchronized = true;
        _lastSyncSuccess = millis();
        Serial.printf("[TimeManager] Time synchronized: %s\n", getISO8601().c_str());
        return ErrorCode::ERR_NONE;
    }
    
    // Step 3: Sync failed
    _synchronized = false;
    Serial.println(F("[TimeManager] Time sync failed"));
    return ErrorHandler::getInstance().logError(ErrorCode::ERR_TIME_SYNC);
}

// =============================================================================
// SECTION 5: LOOP PROCESSING
// =============================================================================

/**
 * @brief Process time tasks (call in loop)
 */
void TimeManager::loop() {
    // Step 1: Check if periodic re-sync is needed
    if (_initialized && (millis() - _lastSyncAttempt) > NTP_SYNC_INTERVAL) {
        // Background sync - NTP library handles this automatically
        // Just update our tracking
        time_t now = time(nullptr);
        if (now > 1000000000) {
            _synchronized = true;
        }
        _lastSyncAttempt = millis();
    }
}

// =============================================================================
// SECTION 6: TIME ACCESSORS
// =============================================================================

/**
 * @brief Get current Unix timestamp
 * @return Unix timestamp (seconds since epoch)
 */
time_t TimeManager::getTimestamp() const {
    return time(nullptr);
}

/**
 * @brief Get current time as ISO8601 string
 * @return ISO8601 formatted string
 */
String TimeManager::getISO8601() const {
    if (!_synchronized) {
        return "1970-01-01T00:00:00Z";
    }
    
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    char buffer[48];
    
    // Format: 2024-01-15T10:30:00+05:30 (max ~30 chars)
    int offsetHours = _timezoneOffset / 3600;
    int offsetMinutes = abs((_timezoneOffset % 3600) / 60);
    
    // Clamp offset hours to reasonable range to prevent buffer overflow
    offsetHours = constrain(offsetHours, -14, 14);
    
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d%+03d:%02d",
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec,
             offsetHours,
             offsetMinutes);
    
    return String(buffer);
}

/**
 * @brief Get current date string
 * @return Date string (e.g., "2024-01-15")
 */
String TimeManager::getDateString() const {
    return getFormatted("%Y-%m-%d");
}

/**
 * @brief Get current time string
 * @return Time string (e.g., "10:30:00")
 */
String TimeManager::getTimeString() const {
    return getFormatted("%H:%M:%S");
}

/**
 * @brief Get formatted datetime string
 * @param format strftime format string
 * @return Formatted string
 */
String TimeManager::getFormatted(const char* format) const {
    if (!_synchronized) {
        return "N/A";
    }
    
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    char buffer[64];
    strftime(buffer, sizeof(buffer), format, timeinfo);
    
    return String(buffer);
}

// =============================================================================
// SECTION 7: UPTIME
// =============================================================================

/**
 * @brief Get device uptime in milliseconds
 * @return Uptime in milliseconds
 */
unsigned long TimeManager::getUptimeMs() const {
    return millis();
}

/**
 * @brief Get device uptime as formatted string
 * @return Uptime string (e.g., "2d 5h 30m 15s")
 */
String TimeManager::getUptimeString() const {
    unsigned long ms = millis();
    unsigned long seconds = ms / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    
    String result = "";
    
    if (days > 0) {
        result += String(days) + "d ";
    }
    if (hours % 24 > 0 || days > 0) {
        result += String(hours % 24) + "h ";
    }
    if (minutes % 60 > 0 || hours > 0) {
        result += String(minutes % 60) + "m ";
    }
    result += String(seconds % 60) + "s";
    
    return result;
}

// =============================================================================
// SECTION 8: TIMEZONE
// =============================================================================

/**
 * @brief Set timezone offset
 * @param offsetSeconds Offset from UTC in seconds
 */
void TimeManager::setTimezoneOffset(int32_t offsetSeconds) {
    _timezoneOffset = offsetSeconds;
    configureNTP();
}

// =============================================================================
// SECTION 9: JSON STATUS
// =============================================================================

/**
 * @brief Get time status as JSON string
 * @return JSON string with time status
 */
String TimeManager::getStatusJson() const {
    JsonDocument doc;
    
    doc["synchronized"] = _synchronized;
    doc["timestamp"] = (unsigned long)getTimestamp();
    doc["iso8601"] = getISO8601();
    doc["date"] = getDateString();
    doc["time"] = getTimeString();
    doc["uptimeMs"] = getUptimeMs();
    doc["uptime"] = getUptimeString();
    doc["timezoneOffset"] = _timezoneOffset;
    doc["ntpServer"] = ConfigManager::getInstance().getSystemConfig().ntpServer;
    
    String output;
    serializeJson(doc, output);
    return output;
}

