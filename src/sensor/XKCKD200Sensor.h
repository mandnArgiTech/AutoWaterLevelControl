/**
 * @file XKCKD200Sensor.h
 * @brief XKC-KD200 Infrared Non-Contact Liquid Level Sensor Driver
 * 
 * The XKC-KD200 is a non-contact infrared liquid level sensor that
 * detects the presence of liquid through container walls (up to 20mm).
 * 
 * This is a POINT-LEVEL sensor (detects presence at a specific height),
 * not a continuous distance sensor.
 * 
 * Specifications:
 * - Working Voltage: DC 5-24V
 * - Output: NPN-NO (Normally Open, active HIGH)
 * - Detection: Through non-metallic walls up to 20mm
 * - Response Time: 500ms
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#ifndef XKCKD200_SENSOR_H
#define XKCKD200_SENSOR_H

#include "ISensor.h"
#include "../config/ConfigManager.h"

#define XKCKD200_DEBOUNCE_MS    100
#define XKCKD200_SAMPLE_COUNT   5

enum class LiquidState {
    NOT_DETECTED,
    DETECTED,
    UNKNOWN
};

class XKCKD200Sensor : public ISensor {
public:
    XKCKD200Sensor(int signalPin, float mountHeightMm = 500.0f, 
                   float tankHeightMm = 1000.0f, bool invertedLogic = false);
    virtual ~XKCKD200Sensor() = default;
    
    // ISensor Interface
    ErrorCode begin() override;
    bool isReady() const override { return _initialized; }
    float readDistanceMm() override;
    float readDistanceAverageMm(uint8_t samples = 5) override;
    SensorReading getReading() override;
    const SensorReading& getLastReading() const override { return _lastReading; }
    SensorType getSensorType() const override { return SensorType::INFRARED; }
    String getSensorTypeName() const override { return "XKC-KD200"; }
    ErrorCode getLastError() const override { return _lastError; }
    float getMinRange() const override { return 0; }
    float getMaxRange() const override { return _tankHeightMm; }
    bool testConnection() override;
    String getStatusJson() const override;
    
    // XKC-KD200 Specific
    LiquidState getLiquidState() const { return _liquidState; }
    bool isLiquidDetected() const { return _liquidState == LiquidState::DETECTED; }
    void setMountHeight(float heightMm) { _mountHeightMm = heightMm; }
    float getMountHeight() const { return _mountHeightMm; }
    void setTankHeight(float heightMm) { _tankHeightMm = heightMm; }
    float getTankHeight() const { return _tankHeightMm; }
    void setInvertedLogic(bool inverted) { _invertedLogic = inverted; }
    bool getRawState() const;

private:
    bool readDebouncedState();
    void updateLiquidState();
    
    int _signalPin;
    float _mountHeightMm;
    float _tankHeightMm;
    bool _invertedLogic;
    bool _initialized;
    LiquidState _liquidState;
    ErrorCode _lastError;
    SensorReading _lastReading;
    unsigned long _lastStateChange;
    bool _lastRawState;
    uint32_t _readCount;
    uint32_t _detectionCount;
    uint32_t _stateChanges;
};

#endif // XKCKD200_SENSOR_H
