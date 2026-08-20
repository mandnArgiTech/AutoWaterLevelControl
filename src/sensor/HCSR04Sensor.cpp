/**
 * @file HCSR04Sensor.cpp
 * @brief HC-SR04 Ultrasonic Distance Sensor Implementation
 * 
 * @author FluidLevelMonitor Project
 * @version 1.1.0
 */

#include "HCSR04Sensor.h"

HCSR04Sensor::HCSR04Sensor(int trigPin, int echoPin)
    : _trigPin(trigPin)
    , _echoPin(echoPin)
    , _temperature(20.0f)
    , _lastMeasureTime(0) {
}

ErrorCode HCSR04Sensor::begin() {
    Serial.println(F("[HC-SR04] Initializing..."));

    pinMode(_trigPin, OUTPUT);
    pinMode(_echoPin, INPUT);
    digitalWrite(_trigPin, LOW);
    delayMicroseconds(2);
    delay(100);

    float test = readRawDistanceMm();
    if (test < 0) {
        Serial.println(F("[HC-SR04] Warning: initial reading failed"));
    } else {
        Serial.printf("[HC-SR04] Test: %.1f mm\n", test);
    }

    _initialized = true;
    Serial.printf("[HC-SR04] TRIG=%d ECHO=%d  Range: %d–%d mm\n",
                  _trigPin, _echoPin, HCSR04_MIN_DISTANCE_MM, HCSR04_MAX_DISTANCE_MM);
    return ErrorCode::ERR_NONE;
}

// ---------------------------------------------------------------------------
// Distance measurement
// ---------------------------------------------------------------------------

unsigned long HCSR04Sensor::measureEchoPulse() {
    unsigned long now = millis();
    if (now - _lastMeasureTime < HCSR04_SETTLING_TIME_MS) {
        delay(HCSR04_SETTLING_TIME_MS - (now - _lastMeasureTime));
    }

    digitalWrite(_trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(_trigPin, HIGH);
    delayMicroseconds(HCSR04_TRIGGER_PULSE_US);
    digitalWrite(_trigPin, LOW);

    unsigned long duration = pulseIn(_echoPin, HIGH, HCSR04_ECHO_TIMEOUT_US);
    _lastMeasureTime = millis();
    return duration;
}

float HCSR04Sensor::getSpeedOfSound() const {
    return (331.3f + 0.606f * _temperature) / 10000.0f; // cm/µs
}

float HCSR04Sensor::readRawDistanceMm() {
    if (!_initialized) {
        _lastError = ErrorCode::ERR_SENSOR_INIT;
        return -1;
    }

    unsigned long duration = measureEchoPulse();
    if (duration == 0) {
        _lastError = ErrorCode::ERR_SENSOR_TIMEOUT;
        _errorCount++;
        ErrorHandler::getInstance().logError(_lastError);
        return -1;
    }

    float distanceMm = (duration * getSpeedOfSound()) / 2.0f * 10.0f;

    if (!validateDistance(distanceMm)) {
        _errorCount++;
        return -1;
    }

    _readCount++;
    _lastError = ErrorCode::ERR_NONE;
    return distanceMm;
}

bool HCSR04Sensor::validateDistance(float distance) {
    if (distance < HCSR04_MIN_DISTANCE_MM) {
        _lastError = ErrorCode::ERR_SENSOR_DISTANCE_MIN;
        return false;
    }
    if (distance > HCSR04_MAX_DISTANCE_MM) {
        _lastError = ErrorCode::ERR_SENSOR_DISTANCE_MAX;
        return false;
    }
    return true;
}

bool HCSR04Sensor::testConnection() {
    return readRawDistanceMm() > 0;
}

String HCSR04Sensor::getStatusJson() const {
    JsonDocument doc;
    doc["type"] = getSensorTypeName();
    doc["initialized"] = _initialized;
    doc["trigPin"] = _trigPin;
    doc["echoPin"] = _echoPin;
    doc["temperature"] = _temperature;
    doc["minRange"] = getMinRange();
    doc["maxRange"] = getMaxRange();
    addFilterStatusJson(doc);
    addReadingStatusJson(doc);

    String out;
    serializeJson(doc, out);
    return out;
}
