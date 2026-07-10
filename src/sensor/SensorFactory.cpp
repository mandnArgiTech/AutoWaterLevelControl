/**
 * @file SensorFactory.cpp
 * @brief Sensor Factory Implementation
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#include "SensorFactory.h"
#include <ArduinoJson.h>

#ifndef DEFAULT_MOUNT_HEIGHT_MM
#define DEFAULT_MOUNT_HEIGHT_MM 500.0f
#endif

// =============================================================================
// SECTION 1: SENSOR CREATION
// =============================================================================

ISensor* SensorFactory::createSensor(const SensorHWConfig& config) {
    String type = normalizeType(config.type);
    
    Serial.printf("[SensorFactory] Creating sensor type: %s\n", type.c_str());
    
    if (type == "US100" || type == "US-100") {
        Serial.printf("[SensorFactory] US-100 on RX=%d, TX=%d\n", config.rxPin, config.txPin);
        return new UltrasonicSensor(config.rxPin, config.txPin);
    }

    if (type == "A02YYUW" || type == "A02-YYUW" || type == "DYP-A02YYUW") {
        Serial.printf("[SensorFactory] A02YYUW on RX=%d, TX=%d\n", config.rxPin, config.txPin);
        return new A02YYUWSensor(config.rxPin, config.txPin);
    }
    
    if (type == "HC_SR04" || type == "HCSR04" || type == "HC-SR04") {
        Serial.printf("[SensorFactory] HC-SR04 on TRIG=%d, ECHO=%d\n", config.trigPin, config.echoPin);
        return new HCSR04Sensor(config.trigPin, config.echoPin);
    }
    
    if (type == "TF_LUNA" || type == "TFLUNA" || type == "TF-LUNA") {
        Serial.printf("[SensorFactory] TF-Luna on RX=%d, TX=%d\n", config.rxPin, config.txPin);
        return new TFLunaSensor(config.rxPin, config.txPin);
    }
    
    if (type == "XKC_KD200" || type == "XKCKD200" || type == "XKC-KD200") {
        Serial.printf("[SensorFactory] XKC-KD200 on PIN=%d, mount=%.1fmm\n", 
                      config.signalPin, config.mountHeightMm);
        return new XKCKD200Sensor(config.signalPin, config.mountHeightMm, 
                                  config.tankHeightMm, config.invertedLogic);
    }
    
    Serial.printf("[SensorFactory] ERROR: Unknown sensor type: %s\n", config.type.c_str());
    return nullptr;
}

// =============================================================================
// SECTION 2: TYPE HELPERS
// =============================================================================

String SensorFactory::normalizeType(const String& typeStr) {
    String upper = typeStr;
    upper.toUpperCase();
    return upper;
}

// =============================================================================
// SECTION 3: DEFAULT CONFIGURATIONS
// =============================================================================

void SensorFactory::getDefaultPins(const String& typeStr, SensorHWConfig& config) {
    config.rxPin = 14;          // D5 / GPIO14 (sensor TX -> ESP RX)
    config.txPin = 12;          // D6 / GPIO12 (sensor RX <- ESP TX)
    config.trigPin = 14;        // D5 / GPIO14
    config.echoPin = 12;        // D6 / GPIO12
    config.signalPin = 14;      // D5 / GPIO14
    config.mountHeightMm = 500.0f;
    config.tankHeightMm = 1704.5f;
    config.invertedLogic = false;
    config.type = typeStr;
}

