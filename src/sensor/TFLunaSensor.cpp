/**
 * @file TFLunaSensor.cpp
 * @brief TF-Luna LiDAR Distance Sensor Implementation
 * 
 * @author FluidLevelMonitor Project
 * @version 1.1.0
 */

#include "TFLunaSensor.h"

TFLunaSensor::TFLunaSensor(int rxPin, int txPin)
    : _serial(nullptr)
    , _rxPin(rxPin)
    , _txPin(txPin)
    , _temperature(25.0f)
    , _signalStrength(0)
    , _rawDistance(0)
    , _checksumErrors(0) {
}

TFLunaSensor::~TFLunaSensor() {
    if (_serial) {
        _serial->end();
        delete _serial;
        _serial = nullptr;
    }
}

ErrorCode TFLunaSensor::begin() {
    Serial.println(F("[TF-Luna] Initializing..."));

    _serial = new SoftwareSerial(_rxPin, _txPin);
    if (!_serial) {
        _lastError = ErrorCode::ERR_SENSOR_INIT;
        return _lastError;
    }

    _serial->begin(TFLUNA_BAUD_RATE);
    delay(100);
    clearBuffer();

    delay(100);
    if (readFrame()) {
        Serial.printf("[TF-Luna] Test: %d cm, strength: %d\n", _rawDistance, _signalStrength);
    } else {
        Serial.println(F("[TF-Luna] Warning: initial reading failed"));
    }

    _initialized = true;
    Serial.printf("[TF-Luna] RX=%d TX=%d  Range: %d–%d mm\n",
                  _rxPin, _txPin, TFLUNA_MIN_DISTANCE_MM, TFLUNA_MAX_DISTANCE_MM);
    return ErrorCode::ERR_NONE;
}

// ---------------------------------------------------------------------------
// Frame reading
// ---------------------------------------------------------------------------

void TFLunaSensor::clearBuffer() {
    while (_serial && _serial->available()) _serial->read();
}

bool TFLunaSensor::readFrame() {
    if (!_serial) {
        _lastError = ErrorCode::ERR_SENSOR_INIT;
        return false;
    }

    uint8_t frame[TFLUNA_FRAME_SIZE];
    uint8_t idx = 0;
    unsigned long start = millis();

    while (millis() - start < TFLUNA_TIMEOUT_MS) {
        if (_serial->available()) {
            uint8_t b = _serial->read();
            if (idx == 0) {
                if (b == TFLUNA_HEADER) frame[idx++] = b;
            } else if (idx == 1) {
                if (b == TFLUNA_HEADER) frame[idx++] = b;
                else idx = 0;
            } else {
                frame[idx++] = b;
                if (idx >= TFLUNA_FRAME_SIZE) return parseFrame(frame);
            }
        }
        yield();
    }

    _lastError = ErrorCode::ERR_SENSOR_TIMEOUT;
    _errorCount++;
    return false;
}

bool TFLunaSensor::parseFrame(uint8_t* frame) {
    if (!validateChecksum(frame)) {
        _checksumErrors++;
        _errorCount++;
        _lastError = ErrorCode::ERR_SENSOR_INVALID_DATA;
        return false;
    }

    _rawDistance     = frame[2] | (frame[3] << 8);
    _signalStrength = frame[4] | (frame[5] << 8);
    int16_t rawTemp = frame[6] | (frame[7] << 8);
    _temperature = rawTemp / 100.0f - 256.0f;

    if (_signalStrength < TFLUNA_MIN_STRENGTH) {
        _lastError = ErrorCode::ERR_SENSOR_INVALID_DATA;
        _errorCount++;
        return false;
    }

    _lastError = ErrorCode::ERR_NONE;
    _readCount++;
    return true;
}

bool TFLunaSensor::validateChecksum(uint8_t* frame) {
    uint8_t sum = 0;
    for (int i = 0; i < 8; i++) sum += frame[i];
    return (sum == frame[8]);
}

// ---------------------------------------------------------------------------
// Distance measurement
// ---------------------------------------------------------------------------

float TFLunaSensor::readRawDistanceMm() {
    if (!_initialized) {
        _lastError = ErrorCode::ERR_SENSOR_INIT;
        return -1;
    }
    if (!readFrame()) return -1;

    float distMm = _rawDistance * 10.0f;

    if (distMm < TFLUNA_MIN_DISTANCE_MM) {
        _lastError = ErrorCode::ERR_SENSOR_DISTANCE_MIN;
        return -1;
    }
    if (distMm > TFLUNA_MAX_DISTANCE_MM) {
        _lastError = ErrorCode::ERR_SENSOR_DISTANCE_MAX;
        return -1;
    }
    return distMm;
}

bool TFLunaSensor::testConnection() {
    return readRawDistanceMm() > 0;
}

String TFLunaSensor::getStatusJson() const {
    JsonDocument doc;
    doc["type"] = getSensorTypeName();
    doc["initialized"] = _initialized;
    doc["rxPin"] = _rxPin;
    doc["txPin"] = _txPin;
    doc["temperature"] = _temperature;
    doc["signalStrength"] = _signalStrength;
    doc["minRange"] = getMinRange();
    doc["maxRange"] = getMaxRange();
    doc["checksumErrors"] = _checksumErrors;
    addFilterStatusJson(doc);
    addReadingStatusJson(doc);

    String out;
    serializeJson(doc, out);
    return out;
}
