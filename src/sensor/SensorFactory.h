/**
 * @file SensorFactory.h
 * @brief Factory for creating sensor instances based on configuration
 * 
 * Supports multiple sensor types:
 * - US-100: Ultrasonic (UART serial)
 * - A02YYUW: DYP-A02YYUW Ultrasonic (UART auto-frame)
 * - HC-SR04: Ultrasonic (trigger/echo)
 * - TF-Luna: LiDAR (UART serial)
 * - XKC-KD200: Infrared point-level (digital)
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#ifndef SENSOR_FACTORY_H
#define SENSOR_FACTORY_H

#include "ISensor.h"
#include "UltrasonicSensor.h"
#include "HCSR04Sensor.h"
#include "TFLunaSensor.h"
#include "XKCKD200Sensor.h"
#include "A02YYUWSensor.h"
#include "../config/ConfigManager.h"

// Note: SensorType enum is defined in ISensor.h
// This factory uses string-based type identification for flexibility

// =============================================================================
// SECTION 1: SENSOR FACTORY CLASS
// =============================================================================

/**
 * @class SensorFactory
 * @brief Factory class for creating sensor instances
 * 
 * Creates appropriate sensor object based on configuration.
 * Supports runtime sensor selection and hot-swapping.
 */
class SensorFactory {
public:
    /**
     * @brief Create sensor instance based on configuration
     * @param config Sensor configuration
     * @return Pointer to ISensor (caller owns memory), nullptr on error
     */
    static ISensor* createSensor(const SensorHWConfig& config);
    
    /**
     * @brief Normalize sensor type string
     * @param typeStr Type string
     * @return Normalized uppercase string
     */
    static String normalizeType(const String& typeStr);
    
    /**
     * @brief Get default pin configuration for sensor type
     * @param typeStr Sensor type string
     * @param config Output configuration
     */
    static void getDefaultPins(const String& typeStr, SensorHWConfig& config);

private:
    SensorFactory() = delete;  // Static class, no instantiation
};

#endif // SENSOR_FACTORY_H
