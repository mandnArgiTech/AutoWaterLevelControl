/**
 * @file ISensor.h
 * @brief Abstract sensor interface for distance measurement
 * 
 * This interface allows the tank level measurement system to be
 * independent of the sensor technology used. Implementations can
 * include:
 * - Ultrasonic sensors (US-100, HC-SR04)
 * - Infrared (IR) sensors
 * - Laser/LIDAR sensors
 * - Time-of-Flight (ToF) sensors
 *
 * @note Call `begin()` once after construction. Implementations are not thread-safe;
 *       only invoke from the main `loop()` on ESP8266.
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#ifndef ISENSOR_H
#define ISENSOR_H

#include <Arduino.h>
#include "../utils/ErrorHandler.h"

// =============================================================================
// SECTION 1: SENSOR READING STRUCTURE
// =============================================================================

/**
 * @struct SensorReading
 * @brief Structure to hold sensor measurement results
 * 
 * All distance measurements are in millimeters internally
 * for precision. Conversion to centimeters is done at display/output.
 */
struct SensorReading {
    float distanceMm;           ///< Distance in millimeters
    float distanceCm;           ///< Distance in centimeters (distanceMm / 10)
    float temperatureC;         ///< Temperature in Celsius (if available)
    bool distanceValid;         ///< Distance reading validity
    bool temperatureValid;      ///< Temperature reading validity
    unsigned long timestamp;    ///< Reading timestamp (millis)
    ErrorCode lastError;        ///< Last error code if any
    
    /**
     * @brief Default constructor
     */
    SensorReading() 
        : distanceMm(0)
        , distanceCm(0)
        , temperatureC(0)
        , distanceValid(false)
        , temperatureValid(false)
        , timestamp(0)
        , lastError(ErrorCode::ERR_NONE) {}
};

// =============================================================================
// SECTION 2: SENSOR TYPE ENUM
// =============================================================================

/**
 * @enum SensorType
 * @brief Types of distance sensors supported
 */
enum class SensorType {
    UNKNOWN = 0,
    ULTRASONIC_US100,       ///< US-100 Ultrasonic sensor (serial mode)
    ULTRASONIC_HCSR04,      ///< HC-SR04 Ultrasonic sensor (pulse mode)
    ULTRASONIC_A02YYUW,     ///< DYP-A02YYUW Ultrasonic (UART auto-frame)
    INFRARED,               ///< Infrared distance sensor
    LASER_LIDAR,            ///< Laser/LIDAR sensor
    TOF                     ///< Time-of-Flight sensor (VL53L0X, etc.)
};

// =============================================================================
// SECTION 3: ABSTRACT SENSOR INTERFACE
// =============================================================================

/**
 * @class ISensor
 * @brief Abstract interface for distance sensors
 * 
 * All distance sensor implementations must inherit from this interface.
 * This allows the tank calculator to work with any sensor technology.
 */
class ISensor {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~ISensor() = default;
    
    /**
     * @brief Initialize the sensor
     * @return ErrorCode indicating success or failure
     */
    virtual ErrorCode begin() = 0;
    
    /**
     * @brief Check if sensor is initialized and ready
     * @return true if ready
     */
    virtual bool isReady() const = 0;
    
    /**
     * @brief Read distance from sensor (single reading)
     * @return Distance in millimeters, or -1 on error
     */
    virtual float readDistanceMm() = 0;
    
    /**
     * @brief Read distance with averaging
     * @param samples Number of samples to average
     * @return Averaged distance in millimeters, or -1 on error
     */
    virtual float readDistanceAverageMm(uint8_t samples = 5) = 0;
    
    /**
     * @brief Read temperature (if sensor supports it)
     * @return Temperature in Celsius, or -999 if not supported
     */
    virtual float readTemperature() { return -999; }
    
    /**
     * @brief Get complete sensor reading
     * @return SensorReading structure with all data
     */
    virtual SensorReading getReading() = 0;
    
    /**
     * @brief Get last sensor reading without new measurement
     * @return Last SensorReading
     */
    virtual const SensorReading& getLastReading() const = 0;
    
    /**
     * @brief Get sensor type
     * @return SensorType enum value
     */
    virtual SensorType getSensorType() const = 0;
    
    /**
     * @brief Get sensor type as string
     * @return Sensor type name
     */
    virtual String getSensorTypeName() const = 0;
    
    /**
     * @brief Get last error code
     * @return ErrorCode
     */
    virtual ErrorCode getLastError() const = 0;
    
    /**
     * @brief Get sensor minimum range in mm
     * @return Minimum measurable distance
     */
    virtual float getMinRange() const = 0;
    
    /**
     * @brief Get sensor maximum range in mm
     * @return Maximum measurable distance
     */
    virtual float getMaxRange() const = 0;
    
    /**
     * @brief Test sensor connectivity
     * @return true if sensor responds
     */
    virtual bool testConnection() = 0;
    
    /**
     * @brief Get sensor status as JSON
     * @return JSON string with sensor status
     */
    virtual String getStatusJson() const = 0;
    
    /**
     * @brief Raw hardware distance in mm — no filter, no calibration offset.
     * @return Distance in mm, or -1 on error / unsupported
     */
    virtual float readRawDistanceMm() { return -1; }

    /**
     * @brief Set calibration offset in mm
     * @param offsetMm Offset to add to readings
     */
    virtual void setCalibrationOffset(float offsetMm) { _calibrationOffset = offsetMm; }
    
    /**
     * @brief Get calibration offset
     * @return Current offset in mm
     */
    virtual float getCalibrationOffset() const { return _calibrationOffset; }

    /**
     * @brief Configure the signal filter pipeline (no-op for non-filtered sensors).
     * Overridden by FilteredSensorBase to set median/moving-avg/Kalman params.
     */
    virtual void configureFilter(bool /*enabled*/, uint8_t /*medianSize*/,
                                 uint8_t /*avgWindow*/, bool /*kalmanEnabled*/,
                                 float /*kalmanQ*/, float /*kalmanR*/) {}

    /** Non-blocking poll (UART parsers). Default no-op. Call every main loop(). */
    virtual void poll() {}

protected:
    float _calibrationOffset = 0;   ///< Calibration offset in mm
};

// =============================================================================
// SECTION 4: HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Convert sensor type to string
 * @param type SensorType enum
 * @return String name
 */
inline String sensorTypeToString(SensorType type) {
    switch (type) {
        case SensorType::ULTRASONIC_US100:  return "US-100 Ultrasonic";
        case SensorType::ULTRASONIC_HCSR04: return "HC-SR04 Ultrasonic";
        case SensorType::ULTRASONIC_A02YYUW: return "A02YYUW Ultrasonic";
        case SensorType::INFRARED:          return "Infrared";
        case SensorType::LASER_LIDAR:       return "Laser/LIDAR";
        case SensorType::TOF:               return "Time-of-Flight";
        default:                            return "Unknown";
    }
}

#endif // ISENSOR_H

