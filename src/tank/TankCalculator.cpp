/**
 * @file TankCalculator.cpp
 * @brief Implementation of water level calculation
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#include "TankCalculator.h"
#include "../sensor/DHT11Ambient.h"
#include "../utils/TimeManager.h"
#include <ESP8266WiFi.h>

static void applyAmbientReading(DHT11Ambient* ambient, WaterLevel& level) {
    if (!ambient) return;
    ambient->update();
    if (ambient->hasValidReading()) {
        level.temperatureC = ambient->getTemperatureC();
        level.humidityPct = ambient->getHumidityPct();
        level.temperatureValid = true;
        level.humidityValid = true;
    }
}

// =============================================================================
// SECTION 1: CONSTRUCTOR
// =============================================================================

/**
 * @brief Constructor with sensor reference
 * @param sensor Reference to ISensor implementation
 */
TankCalculator::TankCalculator(ISensor& sensor, DHT11Ambient* ambient)
    : _sensor(sensor)
    , _ambient(ambient)
    , _initialized(false) {
}

// =============================================================================
// SECTION 2: INITIALIZATION
// =============================================================================

/**
 * @brief Initialize tank calculator
 * @return ErrorCode indicating success or failure
 */
ErrorCode TankCalculator::begin() {
    Serial.println(F("[TankCalculator] Initializing..."));
    
    // Step 1: Validate tank configuration
    ErrorCode result = validateTankConfig();
    if (result != ErrorCode::ERR_NONE) {
        return result;
    }
    
    // Step 2: Print tank configuration
    TankConfig& config = ConfigManager::getInstance().getTankConfig();
    Serial.println(F("[TankCalculator] Tank Configuration:"));
    Serial.printf("  Type: %s\n", config.type.c_str());
    
    if (config.type == "circular") {
        Serial.printf("  Diameter: %.1f mm (%.1f cm)\n", config.diameter, config.diameter/10);
    } else {
        Serial.printf("  Length: %.1f mm, Width: %.1f mm\n", config.length, config.width);
    }
    
    Serial.printf("  Height: %.1f mm (%.1f cm)\n", config.height, config.height/10);
    Serial.printf("  Volume: %.1f liters\n", config.volumeLiters);
    Serial.printf("  Sensor Offset: %.1f mm (%.1f cm)\n", 
                  ConfigManager::getInstance().getSensorConfig().offsetMm,
                  ConfigManager::getInstance().getSensorConfig().offsetMm/10);
    Serial.printf("  Sensor Type: %s\n", _sensor.getSensorTypeName().c_str());
    
    _initialized = true;
    Serial.println(F("[TankCalculator] Initialized successfully"));
    
    return ErrorCode::ERR_NONE;
}

// =============================================================================
// SECTION 3: WATER LEVEL CALCULATION
// =============================================================================

/**
 * @brief Calculate current water level from sensor reading
 * @return WaterLevel structure with calculated values
 */
WaterLevel TankCalculator::calculate() {
    WaterLevel level;
    level.timestamp = millis();
    level.valid = false;
    level.error = ErrorCode::ERR_NONE;
    
    // Step 1: Check initialization
    if (!_initialized) {
        level.error = ErrorCode::ERR_TANK_CONFIG;
        ErrorHandler::getInstance().logError(level.error, "Calculator not initialized");
        _lastLevel = level;
        return level;
    }
    
    // Step 2: Get sensor reading
    SensorReading reading = _sensor.getReading();
    level.distanceMm = reading.distanceMm;
    level.distanceCm = reading.distanceCm;
    if (reading.temperatureValid) {
        level.temperatureC = reading.temperatureC;
        level.temperatureValid = true;
    }
    
    // Step 3: Sensor did not return valid data — skip calculation entirely
    if (!reading.distanceValid) {
        level.error = reading.lastError;
        level.sensorOk = false;
        applyAmbientReading(_ambient, level);
        _lastLevel = level;
        return level;
    }
    
    // Step 4: Sensor is OK — calculate from distance
    WaterLevel calculatedLevel = calculateFromDistance(reading.distanceMm);
    calculatedLevel.sensorOk = true;
    if (reading.temperatureValid) {
        calculatedLevel.temperatureC = reading.temperatureC;
        calculatedLevel.temperatureValid = true;
    }
    applyAmbientReading(_ambient, calculatedLevel);
    calculatedLevel.timestamp = level.timestamp;
    
    _lastLevel = calculatedLevel;
    return calculatedLevel;
}

/**
 * @brief Calculate water level from specific distance in mm
 * @param distanceMm Distance reading in mm
 * @return WaterLevel structure with calculated values
 */
WaterLevel TankCalculator::calculateFromDistance(float distanceMm) {
    WaterLevel level;
    level.timestamp = millis();
    level.distanceMm = distanceMm;
    level.distanceCm = distanceMm / 10.0f;
    level.valid = false;
    level.sensorOk = true;
    level.error = ErrorCode::ERR_NONE;
    
    // Reject invalid distances (error values or clearly out of range)
    if (distanceMm <= 0) {
        level.error = ErrorCode::ERR_SENSOR_INVALID_DATA;
        level.sensorOk = false;
        return level;
    }
    
    TankConfig& tankConfig = ConfigManager::getInstance().getTankConfig();
    SensorConfig& sensorConfig = ConfigManager::getInstance().getSensorConfig();
    
    float airGap = distanceMm - sensorConfig.offsetMm;
    float waterHeight = tankConfig.height - airGap;
    level.waterHeightMm = waterHeight;
    level.waterHeightCm = waterHeight / 10.0f;
    
    if (waterHeight < 0) {
        char msg[32];
        snprintf(msg, sizeof(msg), "%.1fmm", waterHeight);
        level.error = ErrorCode::ERR_TANK_LEVEL_NEGATIVE;
        ErrorHandler::getInstance().logError(level.error, msg);
        level.waterHeightMm = 0;
        level.waterHeightCm = 0;
        level.percentFilled = 0;
        level.percentRemaining = 100;
        level.volumeLiters = 0;
        level.volumeRemaining = tankConfig.volumeLiters;
        level.valid = true;
        return level;
    }
    
    if (waterHeight > tankConfig.height) {
        char msg[32];
        snprintf(msg, sizeof(msg), "%.1fmm", waterHeight);
        level.error = ErrorCode::ERR_TANK_LEVEL_OVERFLOW;
        ErrorHandler::getInstance().logError(level.error, msg);
        level.waterHeightMm = tankConfig.height;
        level.waterHeightCm = tankConfig.height / 10.0f;
    }
    
    // Step 5: Calculate percentage filled
    level.percentFilled = (level.waterHeightMm / tankConfig.height) * 100.0f;
    level.percentFilled = clampPercentage(level.percentFilled);
    
    // Step 6: Calculate remaining percentage
    level.percentRemaining = 100.0f - level.percentFilled;
    
    // Step 7: Calculate volume filled
    level.volumeLiters = calculateVolume(level.waterHeightMm);
    
    // Step 8: Calculate remaining volume
    level.volumeRemaining = tankConfig.volumeLiters - level.volumeLiters;
    if (level.volumeRemaining < 0) level.volumeRemaining = 0;
    
    // Step 9: Mark as valid
    level.valid = true;
    
    // Step 10: Log tank state changes
    TankState state = getTankStateFromPercentage(level.percentFilled);
    if (state == TankState::EMPTY) {
        ErrorHandler::getInstance().logError(ErrorCode::ERR_TANK_EMPTY);
    } else if (state == TankState::FULL) {
        ErrorHandler::getInstance().logError(ErrorCode::ERR_TANK_FULL);
    }
    
    return level;
}

// =============================================================================
// SECTION 4: VOLUME CALCULATION
// =============================================================================

/**
 * @brief Calculate volume for given water height
 * @param waterHeightMm Water height in mm
 * @return Volume in liters
 */
float TankCalculator::calculateVolume(float waterHeightMm) const {
    TankConfig& config = ConfigManager::getInstance().getTankConfig();
    float volumeMm3 = 0;
    
    if (config.type == "circular") {
        // Step 1: Calculate circular tank volume
        // Volume = π * r² * h
        float radius = config.diameter / 2.0f;
        volumeMm3 = PI * radius * radius * waterHeightMm;
    } else if (config.type == "rectangular") {
        // Step 2: Calculate rectangular tank volume
        // Volume = l * w * h
        volumeMm3 = config.length * config.width * waterHeightMm;
    }
    
    // Step 3: Convert mm³ to liters (1 liter = 1,000,000 mm³)
    return volumeMm3 / 1000000.0f;
}

// =============================================================================
// SECTION 5: UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Clamp percentage to valid range with warnings
 * @param percentage Raw percentage value
 * @return Clamped percentage (0-100)
 */
float TankCalculator::clampPercentage(float percentage) {
    if (percentage < 0) {
        return 0;
    }
    if (percentage > 100) {
        return 100;
    }
    return percentage;
}

/**
 * @brief Validate tank configuration
 * @return ErrorCode indicating validation result
 */
ErrorCode TankCalculator::validateTankConfig() const {
    TankConfig& config = ConfigManager::getInstance().getTankConfig();
    
    if (config.height <= 0 || config.height > 10000) {
        char msg[32]; snprintf(msg, sizeof(msg), "height=%.0f", config.height);
        return ErrorHandler::getInstance().logError(ErrorCode::ERR_TANK_CONFIG, msg);
    }
    
    if (config.type == "circular") {
        if (config.diameter <= 0 || config.diameter > 10000) {
            char msg[32]; snprintf(msg, sizeof(msg), "diameter=%.0f", config.diameter);
            return ErrorHandler::getInstance().logError(ErrorCode::ERR_TANK_CONFIG, msg);
        }
    } else if (config.type == "rectangular") {
        if (config.length <= 0 || config.width <= 0) {
            return ErrorHandler::getInstance().logError(ErrorCode::ERR_TANK_CONFIG,
                "Invalid rectangular dimensions");
        }
    } else {
        return ErrorHandler::getInstance().logError(ErrorCode::ERR_TANK_CONFIG,
            config.type.c_str());
    }
    
    return ErrorCode::ERR_NONE;
}

// =============================================================================
// SECTION 6: TANK STATE
// =============================================================================

/**
 * @brief Get current tank state
 * @return TankState enum value
 */
TankState TankCalculator::getTankState() const {
    if (!_lastLevel.valid) {
        return TankState::UNKNOWN;
    }
    return getTankStateFromPercentage(_lastLevel.percentFilled);
}

/**
 * @brief Get tank state from percentage
 * @param percentage Water level percentage
 * @return TankState enum value
 */
TankState TankCalculator::getTankStateFromPercentage(float percentage) {
    if (percentage > 100) return TankState::OVERFLOW;
    if (percentage >= 95) return TankState::FULL;
    if (percentage >= 75) return TankState::LEVEL_HIGH;
    if (percentage >= 25) return TankState::LEVEL_MEDIUM;
    if (percentage >= 5) return TankState::LEVEL_LOW;
    return TankState::EMPTY;
}

/**
 * @brief Convert tank state to string
 * @param state TankState value
 * @return String representation
 */
String TankCalculator::tankStateToString(TankState state) {
    switch (state) {
        case TankState::UNKNOWN:      return "unknown";
        case TankState::EMPTY:        return "empty";
        case TankState::LEVEL_LOW:    return "low";
        case TankState::LEVEL_MEDIUM: return "medium";
        case TankState::LEVEL_HIGH:   return "high";
        case TankState::FULL:         return "full";
        case TankState::OVERFLOW:     return "overflow";
        default:                      return "unknown";
    }
}

// =============================================================================
// SECTION 7: JSON OUTPUT
// =============================================================================

/**
 * @brief Get water level as JSON string
 * @return JSON string with water level data
 */
String TankCalculator::getWaterLevelJson() const {
    JsonDocument doc;
    
    // Percentage values
    doc["percentFilled"] = round(_lastLevel.percentFilled * 10) / 10.0;
    doc["percentRemaining"] = round(_lastLevel.percentRemaining * 10) / 10.0;
    
    // Height in both units
    doc["waterHeightMm"] = round(_lastLevel.waterHeightMm);
    doc["waterHeightCm"] = round(_lastLevel.waterHeightCm * 10) / 10.0;
    
    // Volume
    doc["volumeLiters"] = round(_lastLevel.volumeLiters * 10) / 10.0;
    doc["volumeRemaining"] = round(_lastLevel.volumeRemaining * 10) / 10.0;
    
    // Sensor readings
    doc["distanceMm"] = round(_lastLevel.distanceMm);
    doc["distanceCm"] = round(_lastLevel.distanceCm * 10) / 10.0;
    doc["temperatureValid"] = _lastLevel.temperatureValid;
    if (_lastLevel.temperatureValid) {
        doc["temperatureC"] = _lastLevel.temperatureC;
    }
    if (_lastLevel.humidityValid) {
        doc["humidityPct"] = round(_lastLevel.humidityPct * 10) / 10.0;
    }
    
    // Status
    doc["valid"] = _lastLevel.valid;
    doc["sensorOk"] = _lastLevel.sensorOk;
    doc["state"] = tankStateToString(getTankState());
    doc["timestamp"] = _lastLevel.timestamp;
    
    if (!_lastLevel.sensorOk) {
        doc["sensorStatus"] = "not_responding";
        doc["errorCode"] = static_cast<uint16_t>(_lastLevel.error);
        doc["errorName"] = ErrorHandler::getInstance().getErrorName(_lastLevel.error);
        doc["errorDescription"] = ErrorHandler::getInstance().getErrorDescription(_lastLevel.error);
    } else if (_lastLevel.error != ErrorCode::ERR_NONE) {
        doc["errorCode"] = static_cast<uint16_t>(_lastLevel.error);
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

/**
 * @brief Get water level with device info as JSON for MQTT
 * @param timestamp ISO timestamp string
 * @return JSON string for MQTT publishing
 */
String TankCalculator::getMQTTJson(const String& timestamp) const {
    JsonDocument doc;
    
    MQTTConfig& mqttCfg = ConfigManager::getInstance().getMQTTConfig();
    doc["device"] = ConfigManager::getInstance().getSystemConfig().deviceName;
    doc["deviceTag"] = mqttCfg.deviceName + "_" + String(ESP.getChipId(), HEX);
    doc["deviceName"] = mqttCfg.deviceName;
    doc["chipId"] = String(ESP.getChipId(), HEX);
    doc["timestamp"] = timestamp;
    doc["uptimeMs"] = TimeManager::getInstance().getUptimeMs();
    doc["uptime"] = TimeManager::getInstance().getUptimeString();

    // Water level data
    JsonObject level = doc["level"].to<JsonObject>();
    level["percentFilled"] = round(_lastLevel.percentFilled * 10) / 10.0;
    level["percentRemaining"] = round(_lastLevel.percentRemaining * 10) / 10.0;
    level["waterHeightCm"] = round(_lastLevel.waterHeightCm * 10) / 10.0;
    level["waterHeightMm"] = round(_lastLevel.waterHeightMm);
    level["volumeLiters"] = round(_lastLevel.volumeLiters * 10) / 10.0;
    level["volumeRemaining"] = round(_lastLevel.volumeRemaining * 10) / 10.0;
    level["state"] = tankStateToString(getTankState());
    
    // Sensor data
    JsonObject sensor = doc["sensor"].to<JsonObject>();
    sensor["type"] = _sensor.getSensorTypeName();
    sensor["ok"] = _lastLevel.sensorOk;
    if (_lastLevel.sensorOk) {
        sensor["distanceCm"] = round(_lastLevel.distanceCm * 10) / 10.0;
        sensor["distanceMm"] = round(_lastLevel.distanceMm);
    }
    if (_lastLevel.temperatureValid) {
        sensor["temperatureC"] = _lastLevel.temperatureC;
    }
    if (_lastLevel.humidityValid) {
        sensor["humidityPct"] = round(_lastLevel.humidityPct * 10) / 10.0;
    }
    if (!_lastLevel.sensorOk) {
        sensor["status"] = "not_responding";
        sensor["errorCode"] = static_cast<uint16_t>(_lastLevel.error);
        sensor["errorName"] = ErrorHandler::getInstance().getErrorName(_lastLevel.error);
    }
    sensor["valid"] = _lastLevel.valid;
    
    // Tank info
    TankConfig& tankConfig = ConfigManager::getInstance().getTankConfig();
    JsonObject tank = doc["tank"].to<JsonObject>();
    tank["type"] = tankConfig.type;
    tank["heightCm"] = round(tankConfig.height / 10.0 * 10) / 10.0;
    tank["heightMm"] = tankConfig.height;
    tank["totalVolumeLiters"] = round(tankConfig.volumeLiters * 10) / 10.0;
    
    String output;
    serializeJson(doc, output);
    return output;
}
