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

ISensor* SensorFactory::createSensorByType(const String& typeStr) {
    SensorHWConfig config;
    getDefaultPins(typeStr, config);
    config.type = typeStr;
    return createSensor(config);
}

ISensor* SensorFactory::createSensorFromString(const String& typeStr, int pin1, int pin2, 
                                               float tankHeight) {
    SensorHWConfig config;
    config.type = typeStr;
    
    String type = normalizeType(typeStr);
    
    if (type == "US100" || type == "US-100" || type == "TF_LUNA" || type == "TFLUNA" || type == "TF-LUNA") {
        config.rxPin = pin1;
        config.txPin = pin2;
    } else if (type == "HC_SR04" || type == "HCSR04" || type == "HC-SR04") {
        config.trigPin = pin1;
        config.echoPin = pin2;
    } else if (type == "XKC_KD200" || type == "XKCKD200" || type == "XKC-KD200") {
        config.signalPin = pin1;
        config.mountHeightMm = DEFAULT_MOUNT_HEIGHT_MM;
        config.tankHeightMm = tankHeight;
        config.invertedLogic = false;
    } else {
        config.rxPin = pin1;
        config.txPin = pin2;
    }
    
    return createSensor(config);
}

// =============================================================================
// SECTION 2: TYPE HELPERS
// =============================================================================

String SensorFactory::normalizeType(const String& typeStr) {
    String upper = typeStr;
    upper.toUpperCase();
    return upper;
}

bool SensorFactory::isValidType(const String& typeStr) {
    String type = normalizeType(typeStr);
    return (type == "US100" || type == "US-100" ||
            type == "HC_SR04" || type == "HCSR04" || type == "HC-SR04" ||
            type == "TF_LUNA" || type == "TFLUNA" || type == "TF-LUNA" ||
            type == "XKC_KD200" || type == "XKCKD200" || type == "XKC-KD200");
}

// =============================================================================
// SECTION 3: DEFAULT CONFIGURATIONS
// =============================================================================

void SensorFactory::getDefaultPins(const String& typeStr, SensorHWConfig& config) {
    config.rxPin = 5;           // D1 / GPIO5
    config.txPin = 4;           // D2 / GPIO4
    config.trigPin = 5;         // D1 / GPIO5
    config.echoPin = 4;         // D2 / GPIO4
    config.signalPin = 14;      // D5 / GPIO14
    config.mountHeightMm = 500.0f;
    config.tankHeightMm = 1704.5f;
    config.invertedLogic = false;
    config.type = typeStr;
}

String SensorFactory::getSupportedTypes() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    
    JsonObject us100 = arr.add<JsonObject>();
    us100["type"] = "US100";
    us100["name"] = "US-100 Ultrasonic";
    us100["interface"] = "UART";
    us100["range"] = "20-4500mm";
    
    JsonObject hcsr04 = arr.add<JsonObject>();
    hcsr04["type"] = "HC_SR04";
    hcsr04["name"] = "HC-SR04 Ultrasonic";
    hcsr04["interface"] = "Trigger/Echo";
    hcsr04["range"] = "20-4000mm";
    
    JsonObject tfluna = arr.add<JsonObject>();
    tfluna["type"] = "TF_LUNA";
    tfluna["name"] = "TF-Luna LiDAR";
    tfluna["interface"] = "UART";
    tfluna["range"] = "200-8000mm";
    
    JsonObject xkckd200 = arr.add<JsonObject>();
    xkckd200["type"] = "XKC_KD200";
    xkckd200["name"] = "XKC-KD200 IR Level";
    xkckd200["interface"] = "Digital";
    xkckd200["range"] = "Point-level";
    
    String output;
    serializeJson(doc, output);
    return output;
}
