/**
 * @file TimeManager.h
 * @brief NTP time synchronization for ESP8266
 * 
 * This module provides:
 * - NTP time synchronization
 * - Timezone handling
 * - Formatted timestamp generation
 * - Uptime tracking
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <time.h>
#include "../config/ConfigManager.h"
#include "../utils/ErrorHandler.h"

// =============================================================================
// SECTION 1: TIME CONSTANTS
// =============================================================================

#define NTP_SYNC_INTERVAL       3600000     ///< Re-sync every hour (ms)
#define NTP_SYNC_TIMEOUT        10000       ///< Sync timeout (ms)

// =============================================================================
// SECTION 2: TIME MANAGER CLASS
// =============================================================================

/**
 * @class TimeManager
 * @brief Manages time synchronization via NTP
 * 
 * Features:
 * - NTP synchronization
 * - Timezone offset support
 * - ISO8601 timestamp generation
 * - Uptime tracking
 */
class TimeManager {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to TimeManager instance
     */
    static TimeManager& getInstance();
    
    /**
     * @brief Initialize time manager
     * @return ErrorCode indicating success or failure
     */
    ErrorCode begin();
    
    /**
     * @brief Process time tasks (call in loop)
     */
    void loop();
    
    /**
     * @brief Force NTP synchronization
     * @return ErrorCode indicating success or failure
     */
    ErrorCode syncTime();
    
    /**
     * @brief Check if time is synchronized
     * @return true if synchronized
     */
    bool isSynchronized() const { return _synchronized; }
    
    /**
     * @brief Get current Unix timestamp
     * @return Unix timestamp (seconds since epoch)
     */
    time_t getTimestamp() const;
    
    /**
     * @brief Get current time as ISO8601 string
     * @return ISO8601 formatted string (e.g., "2024-01-15T10:30:00+05:30")
     */
    String getISO8601() const;
    
    /**
     * @brief Get current date string
     * @return Date string (e.g., "2024-01-15")
     */
    String getDateString() const;
    
    /**
     * @brief Get current time string
     * @return Time string (e.g., "10:30:00")
     */
    String getTimeString() const;
    
    /**
     * @brief Get formatted datetime string
     * @param format strftime format string
     * @return Formatted string
     */
    String getFormatted(const char* format) const;
    
    /**
     * @brief Get device uptime in milliseconds
     * @return Uptime in milliseconds
     */
    unsigned long getUptimeMs() const;
    
    /**
     * @brief Get device uptime as formatted string
     * @return Uptime string (e.g., "2d 5h 30m 15s")
     */
    String getUptimeString() const;
    
    /**
     * @brief Get time status as JSON
     * @return JSON string with time status
     */
    String getStatusJson() const;
    
    /**
     * @brief Set timezone offset
     * @param offsetSeconds Offset from UTC in seconds
     */
    void setTimezoneOffset(int32_t offsetSeconds);

private:
    // Private constructor for singleton
    TimeManager();
    
    // Delete copy constructor and assignment
    TimeManager(const TimeManager&) = delete;
    TimeManager& operator=(const TimeManager&) = delete;
    
    // Configure NTP
    void configureNTP();
    
    // Member variables
    bool _initialized;              ///< Initialization flag
    bool _synchronized;             ///< Sync status
    unsigned long _lastSyncAttempt; ///< Last sync attempt time
    unsigned long _lastSyncSuccess; ///< Last successful sync time
    int32_t _timezoneOffset;        ///< Timezone offset in seconds
};

#endif // TIME_MANAGER_H

