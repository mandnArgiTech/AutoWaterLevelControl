/**
 * @file UltrasonicSensor.cpp
 * @brief US-100 ultrasonic sensor driver implementation
 * 
 * @author FluidLevelMonitor Project
 * @version 1.1.0
 */

#include "UltrasonicSensor.h"

UltrasonicSensor::UltrasonicSensor(int rxPin, int txPin)
    : _serial(nullptr)
    , _rxPin(rxPin)
    , _txPin(txPin)
    , _timeout(US100_TIMEOUT_MS) {
}

UltrasonicSensor::~UltrasonicSensor() {
    if (_serial) {
        _serial->end();
        delete _serial;
        _serial = nullptr;
    }
}

ErrorCode UltrasonicSensor::begin() {
    Serial.println(F("[US-100] Initializing..."));

    if (_serial) {
        _serial->end();
        delete _serial;
    }
    _serial = new SoftwareSerial(_rxPin, _txPin);
    if (!_serial) {
        _lastError = ErrorCode::ERR_SYSTEM_MEMORY;
        return ErrorHandler::getInstance().logError(_lastError);
    }

    _serial->begin(US100_BAUD_RATE);
    delay(100);
    clearBuffer();

    if (!testConnection()) {
        Serial.println(F("[US-100] Warning: initial test failed — sensor may not be connected"));
    }

    _initialized = true;
    Serial.printf("[US-100] RX=%d TX=%d  Range: %d–%d mm\n",
                  _rxPin, _txPin, US100_MIN_DISTANCE, US100_MAX_DISTANCE);
    return ErrorCode::ERR_NONE;
}

float UltrasonicSensor::readRawDistanceMm() {
    if (!_initialized || !_serial) {
        _lastError = ErrorCode::ERR_SENSOR_INIT;
        ErrorHandler::getInstance().logError(_lastError);
        return -1;
    }

    clearBuffer();
    sendCommand(US100_CMD_DISTANCE);

    if (!waitForBytes(2)) {
        _lastError = ErrorCode::ERR_SENSOR_TIMEOUT;
        _errorCount++;
        ErrorHandler::getInstance().logError(_lastError);
        return -1;
    }

    uint8_t hi = _serial->read();
    uint8_t lo = _serial->read();
    float distance = (hi * 256.0f) + lo;

    if (!validateDistance(distance)) {
        _errorCount++;
        return -1;
    }

    _readCount++;
    _lastError = ErrorCode::ERR_NONE;
    return distance;
}

float UltrasonicSensor::readTemperature() {
    if (!_initialized || !_serial) return -999;

    clearBuffer();
    sendCommand(US100_CMD_TEMP);

    if (!waitForBytes(1)) return -999;

    float temp = _serial->read() - US100_TEMP_OFFSET;
    return (temp >= -40 && temp <= 85) ? temp : -999;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void UltrasonicSensor::sendCommand(uint8_t cmd) {
    _serial->flush();
    _serial->write(cmd);
}

bool UltrasonicSensor::waitForBytes(uint8_t count) {
    unsigned long start = millis();
    while (_serial->available() < count) {
        if (millis() - start > _timeout) return false;
        yield();
    }
    return true;
}

bool UltrasonicSensor::validateDistance(float distance) {
    if (distance < US100_MIN_DISTANCE) {
        _lastError = ErrorCode::ERR_SENSOR_DISTANCE_MIN;
        char msg[24]; snprintf(msg, sizeof(msg), "%.0fmm", distance);
        ErrorHandler::getInstance().logError(_lastError, msg);
        return false;
    }
    if (distance > US100_MAX_DISTANCE) {
        _lastError = ErrorCode::ERR_SENSOR_DISTANCE_MAX;
        char msg[24]; snprintf(msg, sizeof(msg), "%.0fmm", distance);
        ErrorHandler::getInstance().logError(_lastError, msg);
        return false;
    }
    return true;
}

void UltrasonicSensor::clearBuffer() {
    while (_serial->available()) _serial->read();
}

bool UltrasonicSensor::testConnection() {
    clearBuffer();
    sendCommand(US100_CMD_DISTANCE);
    if (waitForBytes(2)) {
        _serial->read();
        _serial->read();
        return true;
    }
    return false;
}

String UltrasonicSensor::getStatusJson() const {
    JsonDocument doc;
    doc["type"] = getSensorTypeName();
    doc["initialized"] = _initialized;
    doc["rxPin"] = _rxPin;
    doc["txPin"] = _txPin;
    doc["timeout"] = _timeout;
    doc["minRange"] = getMinRange();
    doc["maxRange"] = getMaxRange();
    addFilterStatusJson(doc);
    addReadingStatusJson(doc);

    String out;
    serializeJson(doc, out);
    return out;
}
