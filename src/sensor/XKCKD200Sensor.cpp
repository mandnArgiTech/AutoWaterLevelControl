/**
 * @file XKCKD200Sensor.cpp
 * @brief XKC-KD200 Infrared Non-Contact Liquid Level Sensor Implementation
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#include "XKCKD200Sensor.h"
#include <ArduinoJson.h>

// =============================================================================
// SECTION 1: CONSTRUCTOR & INITIALIZATION
// =============================================================================

XKCKD200Sensor::XKCKD200Sensor(int signalPin, float mountHeightMm, 
                               float tankHeightMm, bool invertedLogic)
    : _signalPin(signalPin)
    , _mountHeightMm(mountHeightMm)
    , _tankHeightMm(tankHeightMm)
    , _invertedLogic(invertedLogic)
    , _initialized(false)
    , _liquidState(LiquidState::UNKNOWN)
    , _lastError(ErrorCode::ERR_NONE)
    , _lastStateChange(0)
    , _lastRawState(false)
    , _readCount(0)
    , _detectionCount(0)
    , _stateChanges(0) {
}

ErrorCode XKCKD200Sensor::begin() {
    Serial.println("[XKC-KD200] Initializing...");
    
    pinMode(_signalPin, INPUT_PULLUP);
    delay(100);
    
    _lastRawState = digitalRead(_signalPin);
    _lastStateChange = millis();
    updateLiquidState();
    
    _initialized = true;
    Serial.printf("[XKC-KD200] Initialized on pin %d\n", _signalPin);
    Serial.printf("[XKC-KD200] Mount height: %.1f mm, Tank: %.1f mm\n", _mountHeightMm, _tankHeightMm);
    Serial.printf("[XKC-KD200] Initial state: %s\n", 
                  _liquidState == LiquidState::DETECTED ? "LIQUID" : "NO LIQUID");
    
    return ErrorCode::ERR_NONE;
}

// =============================================================================
// SECTION 2: STATE READING
// =============================================================================

bool XKCKD200Sensor::getRawState() const {
    return digitalRead(_signalPin);
}

bool XKCKD200Sensor::readDebouncedState() {
    int highCount = 0;
    for (int i = 0; i < XKCKD200_SAMPLE_COUNT; i++) {
        if (digitalRead(_signalPin)) highCount++;
        delayMicroseconds(100);
    }
    return (highCount > XKCKD200_SAMPLE_COUNT / 2);
}

void XKCKD200Sensor::updateLiquidState() {
    bool currentState = readDebouncedState();
    unsigned long now = millis();
    
    if (currentState != _lastRawState) {
        if (now - _lastStateChange >= XKCKD200_DEBOUNCE_MS) {
            _lastRawState = currentState;
            _lastStateChange = now;
            _stateChanges++;
        } else {
            return;
        }
    }
    
    bool liquidDetected = _invertedLogic ? !_lastRawState : _lastRawState;
    LiquidState newState = liquidDetected ? LiquidState::DETECTED : LiquidState::NOT_DETECTED;
    
    if (newState != _liquidState) {
        _liquidState = newState;
        if (liquidDetected) _detectionCount++;
    }
}

// =============================================================================
// SECTION 3: DISTANCE INTERFACE
// =============================================================================

float XKCKD200Sensor::readDistanceMm() {
    if (!_initialized) {
        _lastError = ErrorCode::ERR_SENSOR_INIT;
        return -1;
    }
    
    updateLiquidState();
    _readCount++;
    
    // Convert point detection to distance
    // When detected: water is at mount height, distance from top = tankHeight - mountHeight
    // When not detected: water below sensor, report full tank height
    if (_liquidState == LiquidState::DETECTED) {
        return _tankHeightMm - _mountHeightMm;
    } else {
        return _tankHeightMm;
    }
}

float XKCKD200Sensor::readDistanceAverageMm(uint8_t samples) {
    // Point level sensor - no averaging needed, just return current reading
    return readDistanceMm();
}

SensorReading XKCKD200Sensor::getReading() {
    SensorReading reading;
    reading.timestamp = millis();
    reading.temperatureC = 25.0f;
    reading.temperatureValid = false;
    
    float distance = readDistanceMm();
    
    if (distance >= 0) {
        reading.distanceMm = distance;
        reading.distanceCm = distance / 10.0f;
        reading.distanceValid = true;
        reading.lastError = ErrorCode::ERR_NONE;
    } else {
        reading.distanceMm = 0;
        reading.distanceCm = 0;
        reading.distanceValid = false;
        reading.lastError = _lastError;
    }
    
    _lastReading = reading;
    return reading;
}

bool XKCKD200Sensor::testConnection() {
    // For digital sensor, just verify we can read the pin
    getRawState();
    return true;
}

String XKCKD200Sensor::getStatusJson() const {
    JsonDocument doc;
    
    doc["type"] = getSensorTypeName();
    doc["initialized"] = _initialized;
    doc["signalPin"] = _signalPin;
    doc["mountHeightMm"] = _mountHeightMm;
    doc["tankHeightMm"] = _tankHeightMm;
    doc["invertedLogic"] = _invertedLogic;
    doc["rawState"] = getRawState();
    doc["liquidDetected"] = (_liquidState == LiquidState::DETECTED);
    doc["readCount"] = _readCount;
    doc["detectionCount"] = _detectionCount;
    doc["stateChanges"] = _stateChanges;
    
    JsonObject last = doc["lastReading"].to<JsonObject>();
    last["distanceMm"] = _lastReading.distanceMm;
    last["valid"] = _lastReading.distanceValid;
    last["timestamp"] = _lastReading.timestamp;
    
    String output;
    serializeJson(doc, output);
    return output;
}
