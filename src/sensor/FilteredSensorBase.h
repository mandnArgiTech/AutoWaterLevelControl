/**
 * @file FilteredSensorBase.h
 * @brief Base class for sensors with integrated signal filtering
 * 
 * Provides common implementations for filtered distance sensors,
 * eliminating code duplication across US-100, HC-SR04, and TF-Luna.
 * 
 * Subclasses only need to implement hardware-specific methods:
 * - readRawDistanceMm() - low-level sensor communication
 * - begin()             - hardware initialization
 * - testConnection()    - connectivity check
 * - getStatusJson()     - sensor-specific status
 * - getSensorType/Name/MinRange/MaxRange
 * 
 * @author FluidLevelMonitor Project
 * @version 1.1.0
 */

#ifndef FILTERED_SENSOR_BASE_H
#define FILTERED_SENSOR_BASE_H

#include "ISensor.h"
#include "SensorFilter.h"
#include <ArduinoJson.h>

class FilteredSensorBase : public ISensor {
public:
    FilteredSensorBase();
    virtual ~FilteredSensorBase() = default;

    // --- ISensor common implementations ---
    bool isReady() const override { return _initialized; }
    float readDistanceMm() override;
    float readDistanceAverageMm(uint8_t samples = 5) override;
    SensorReading getReading() override;
    const SensorReading& getLastReading() const override { return _lastReading; }
    ErrorCode getLastError() const override { return _lastError; }

    void configureFilter(bool enabled, uint8_t medianSize, uint8_t avgWindow,
                         bool kalmanEnabled, float kalmanQ, float kalmanR) override;

    // --- Subclass must implement ---
    virtual float readRawDistanceMm() = 0;

    // --- Filter accessors ---
    FluidLevelFilter& getFilter() { return _filter; }
    const FluidLevelFilter& getFilter() const { return _filter; }
    void setFilterEnabled(bool enable) { _filterEnabled = enable; }
    bool isFilterEnabled() const { return _filterEnabled; }

    // --- Statistics ---
    uint32_t getReadCount() const { return _readCount; }
    uint32_t getErrorCount() const { return _errorCount; }

protected:
    /**
     * @brief Delay between samples in readDistanceAverageMm (ms).
     * Override in subclasses needing different settling times.
     */
    virtual uint16_t getSampleDelay() const { return 20; }

    /** Append filter status fields to a JSON document (for getStatusJson) */
    void addFilterStatusJson(JsonDocument& doc) const;

    /** Append last-reading and statistics fields to a JSON document */
    void addReadingStatusJson(JsonDocument& doc) const;

    bool _initialized;
    ErrorCode _lastError;
    SensorReading _lastReading;
    FluidLevelFilter _filter;
    bool _filterEnabled;
    uint32_t _readCount;
    uint32_t _errorCount;
};

#endif // FILTERED_SENSOR_BASE_H
