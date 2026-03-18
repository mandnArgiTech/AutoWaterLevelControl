/**
 * @file FilteredSensorBase.cpp
 * @brief Common implementations for filtered distance sensors
 * 
 * @author FluidLevelMonitor Project
 * @version 1.1.0
 */

#include "FilteredSensorBase.h"

FilteredSensorBase::FilteredSensorBase()
    : _initialized(false)
    , _lastError(ErrorCode::ERR_NONE)
    , _filter(MEDIAN_FILTER_SIZE, MOVING_AVG_WINDOW_SIZE)
    , _filterEnabled(true)
    , _readCount(0)
    , _errorCount(0) {
    _calibrationOffset = 0;
}

/**
 * @brief Read distance with filtering applied.
 * Calls subclass readRawDistanceMm(), then runs the 3-stage filter.
 */
float FilteredSensorBase::readDistanceMm() {
    float raw = readRawDistanceMm();
    if (raw < 0 || !_filterEnabled) {
        return raw;
    }
    return _filter.filter(raw);
}

/**
 * @brief Average multiple filtered readings.
 * Samples are taken with getSampleDelay() between them.
 */
float FilteredSensorBase::readDistanceAverageMm(uint8_t samples) {
    if (samples == 0) samples = 1;
    if (samples > 20) samples = 20;

    float sum = 0;
    uint8_t validCount = 0;

    for (uint8_t i = 0; i < samples; i++) {
        float dist = readDistanceMm();
        if (dist > 0) {
            sum += dist;
            validCount++;
        }
        if (i < samples - 1) {
            delay(getSampleDelay());
        }
    }

    if (validCount == 0) {
        _lastError = ErrorCode::ERR_SENSOR_INVALID_DATA;
        ErrorHandler::getInstance().logError(_lastError, "All samples invalid");
        return -1;
    }
    return sum / validCount;
}

/**
 * @brief Build a complete SensorReading using a single filtered measurement.
 */
SensorReading FilteredSensorBase::getReading() {
    SensorReading reading;
    reading.timestamp = millis();

    reading.temperatureC = readTemperature();
    reading.temperatureValid = (reading.temperatureC > -999);

    float distance = readDistanceMm();
    if (distance > 0) {
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

/**
 * @brief Configure the 3-stage filter pipeline.
 * Called polymorphically from main.cpp — no type casts required.
 */
void FilteredSensorBase::configureFilter(bool enabled, uint8_t medianSize,
                                         uint8_t avgWindow, bool kalmanEnabled,
                                         float kalmanQ, float kalmanR) {
    _filterEnabled = enabled;
    _filter.setMedianSize(medianSize);
    _filter.setMovingAvgWindow(avgWindow);
    _filter.enableKalmanFilter(kalmanEnabled);
    _filter.setKalmanProcessNoise(kalmanQ);
    _filter.setKalmanMeasurementNoise(kalmanR);

    Serial.printf("[Filter] %s (M:%d A:%d K:%s Q:%.3f R:%.3f)\n",
                  enabled ? "ON" : "OFF", medianSize, avgWindow,
                  kalmanEnabled ? "ON" : "OFF", kalmanQ, kalmanR);
}

void FilteredSensorBase::addFilterStatusJson(JsonDocument& doc) const {
    JsonObject f = doc["filter"].to<JsonObject>();
    f["enabled"] = _filterEnabled;
    f["ready"] = _filter.isReady();
    f["medianSize"] = _filter.getMedianFilter().getSize();
    f["movingAvgWindow"] = _filter.getMovingAvgFilter().getWindowSize();
    f["kalmanEnabled"] = _filter.isKalmanEnabled();
    f["kalmanProcessNoise"] = _filter.getKalmanFilter().getProcessNoise();
    f["kalmanMeasureNoise"] = _filter.getKalmanFilter().getMeasurementNoise();
    f["kalmanGain"] = _filter.getKalmanFilter().getKalmanGain();
}

void FilteredSensorBase::addReadingStatusJson(JsonDocument& doc) const {
    doc["calibrationOffset"] = _calibrationOffset;
    doc["calibrationOffsetCm"] = _calibrationOffset / 10.0f;
    doc["readCount"] = _readCount;
    doc["errorCount"] = _errorCount;
    doc["errorRate"] = (_readCount > 0)
                       ? (float)_errorCount / _readCount * 100.0f
                       : 0.0f;

    JsonObject lr = doc["lastReading"].to<JsonObject>();
    lr["distanceMm"] = _lastReading.distanceMm;
    lr["distanceCm"] = _lastReading.distanceCm;
    lr["valid"] = _lastReading.distanceValid;
    lr["timestamp"] = _lastReading.timestamp;
}
