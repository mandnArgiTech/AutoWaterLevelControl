/**
 * @file TankCalculator.h
 * @brief Water level calculation for tank monitoring
 * 
 * This module provides:
 * - Water level percentage calculation (filled and remaining)
 * - Water height in mm and cm
 * - Volume calculation in liters
 * - Support for circular and rectangular tanks
 * - Sensor-agnostic design using ISensor interface
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#ifndef TANK_CALCULATOR_H
#define TANK_CALCULATOR_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../config/ConfigManager.h"
#include "../sensor/ISensor.h"
#include "../utils/ErrorHandler.h"

// =============================================================================
// SECTION 1: WATER LEVEL STRUCTURE
// =============================================================================

/**
 * @struct WaterLevel
 * @brief Structure containing calculated water level data
 */
struct WaterLevel {
    // Percentage values
    float percentFilled;        ///< Water level as percentage filled (0-100)
    float percentRemaining;     ///< Remaining capacity percentage (0-100)
    
    // Height measurements
    float waterHeightMm;        ///< Water height in mm
    float waterHeightCm;        ///< Water height in cm
    
    // Volume
    float volumeLiters;         ///< Current water volume in liters
    float volumeRemaining;      ///< Remaining capacity in liters
    
    // Sensor data
    float distanceMm;           ///< Raw sensor distance reading in mm
    float distanceCm;           ///< Raw sensor distance in cm
    float temperatureC;         ///< Temperature reading
    
    // Status
    bool valid;                 ///< Calculation validity flag
    bool sensorOk;              ///< Sensor responded with valid data
    unsigned long timestamp;    ///< Measurement timestamp
    ErrorCode error;            ///< Error code if any
    
    WaterLevel() 
        : percentFilled(0), percentRemaining(100)
        , waterHeightMm(0), waterHeightCm(0)
        , volumeLiters(0), volumeRemaining(0)
        , distanceMm(0), distanceCm(0), temperatureC(0)
        , valid(false), sensorOk(false), timestamp(0)
        , error(ErrorCode::ERR_NONE) {}
};

// =============================================================================
// SECTION 2: TANK STATE ENUM
// =============================================================================

/**
 * @enum TankState
 * @brief Tank status based on water level
 */
enum class TankState {
    UNKNOWN = 0,    ///< Unable to determine
    EMPTY,          ///< 0-5% - Tank is empty
    LEVEL_LOW,      ///< 5-25% - Low water level
    LEVEL_MEDIUM,   ///< 25-75% - Medium water level
    LEVEL_HIGH,     ///< 75-95% - High water level
    FULL,           ///< 95-100% - Tank is full
    OVERFLOW        ///< >100% - Possible overflow or sensor error
};

// =============================================================================
// SECTION 3: TANK CALCULATOR CLASS
// =============================================================================

/**
 * @class TankCalculator
 * @brief Calculates water level from sensor readings (sensor-agnostic)
 * 
 * The sensor is mounted at the top of the tank, looking down.
 * Distance measurement represents the air gap between sensor and water surface.
 * 
 * Calculation:
 * - Air gap = sensor distance - sensor offset
 * - Water height = tank height - air gap
 * - Percentage filled = (water height / tank height) * 100
 * - Percentage remaining = 100 - percentage filled
 */
class TankCalculator {
public:
    /**
     * @brief Constructor with sensor reference
     * @param sensor Reference to ISensor implementation
     */
    explicit TankCalculator(ISensor& sensor);
    
    /**
     * @brief Initialize calculator
     * @return ErrorCode indicating success or failure
     */
    ErrorCode begin();
    
    /**
     * @brief Calculate current water level
     * @return WaterLevel structure with calculated values
     */
    WaterLevel calculate();
    
    /**
     * @brief Calculate water level from specific distance in mm
     * @param distanceMm Distance reading in mm
     * @return WaterLevel structure with calculated values
     */
    WaterLevel calculateFromDistance(float distanceMm);
    
    /**
     * @brief Calculate water level from distance in cm
     * @param distanceCm Distance reading in cm
     * @return WaterLevel structure with calculated values
     */
    WaterLevel calculateFromDistanceCm(float distanceCm);
    
    /**
     * @brief Get last calculated water level
     * @return Last WaterLevel structure
     */
    const WaterLevel& getLastLevel() const { return _lastLevel; }
    
    /**
     * @brief Get current tank state
     * @return TankState enum value
     */
    TankState getTankState() const;
    
    /**
     * @brief Get tank state from percentage
     * @param percentage Water level percentage
     * @return TankState enum value
     */
    static TankState getTankStateFromPercentage(float percentage);
    
    /**
     * @brief Convert tank state to string
     * @param state TankState value
     * @return String representation
     */
    static String tankStateToString(TankState state);
    
    /**
     * @brief Calculate volume for given water height
     * @param waterHeightMm Water height in mm
     * @return Volume in liters
     */
    float calculateVolume(float waterHeightMm) const;
    
    /**
     * @brief Calculate water height from percentage
     * @param percentage Water level percentage
     * @return Water height in mm
     */
    float percentageToHeight(float percentage) const;
    
    /**
     * @brief Get water level as JSON
     * @return JSON string with water level data
     */
    String getWaterLevelJson() const;
    
    /**
     * @brief Get water level with device info as JSON
     * @param timestamp ISO timestamp string
     * @return JSON string for MQTT publishing
     */
    String getMQTTJson(const String& timestamp) const;
    
    /**
     * @brief Validate tank configuration
     * @return ErrorCode indicating validation result
     */
    ErrorCode validateTankConfig() const;
    
    /**
     * @brief Get effective tank height (accounting for sensor offset)
     * @return Effective height in mm
     */
    float getEffectiveTankHeight() const;
    
    /**
     * @brief Get reference to the sensor
     * @return Reference to ISensor
     */
    ISensor& getSensor() { return _sensor; }

private:
    /**
     * @brief Clamp percentage to valid range with warnings
     * @param percentage Raw percentage value
     * @return Clamped percentage (0-100)
     */
    float clampPercentage(float percentage);
    
    ISensor& _sensor;           ///< Reference to sensor (ISensor interface)
    WaterLevel _lastLevel;      ///< Last calculated level
    bool _initialized;          ///< Initialization flag
};

#endif // TANK_CALCULATOR_H
