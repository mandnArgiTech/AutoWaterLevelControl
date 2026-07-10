/**
 * @file BatteryMonitor.cpp
 * @brief Proven D1 Mini A0 single-cell voltage + calibration (from ADCA1115Calibration)
 */
#include "BatteryMonitor.h"
#include "adc_scaling.h"

BatteryMonitor& BatteryMonitor::getInstance() {
    static BatteryMonitor instance;
    return instance;
}

BatteryMonitor::BatteryMonitor()
    : _cellsInSeries(1)
    , _calibrationOffset(0.0f)
    , _thresholdFull(BATTERY_CELL_VOLTAGE_FULL)
    , _thresholdLow(BATTERY_CELL_VOLTAGE_LOW)
    , _thresholdCritical(BATTERY_CELL_VOLTAGE_EMPTY)
    , _thresholdRecharge(BATTERY_CELL_VOLTAGE_RECHARGE)
    , _cachedVoltage(0.0f)
    , _cachedVoltageMs(0) {
}

void BatteryMonitor::begin(uint8_t cellsInSeries) {
    // Proven project always measures a single cell (never a multi-cell pack).
    (void)cellsInSeries;
    _cellsInSeries = 1;
    _thresholdFull = BATTERY_CELL_VOLTAGE_FULL;
    _thresholdLow = BATTERY_CELL_VOLTAGE_LOW;
    _thresholdCritical = BATTERY_CELL_VOLTAGE_EMPTY;
    _thresholdRecharge = BATTERY_CELL_VOLTAGE_RECHARGE;
}

void BatteryMonitor::setCalibrationOffset(float offset) {
    _calibrationOffset = offset;
}

void BatteryMonitor::calibrate(float measuredActual, float measuredReading) {
    _calibrationOffset = measuredActual - measuredReading;
}

int BatteryMonitor::readAnalogRawAveraged(uint8_t samples) const {
    if (samples == 0) samples = 1;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < samples; i++) {
        sum += analogRead(A0);
        if (i + 1 < samples) {
            delay(2);
        }
    }
    return (int)(sum / samples);
}

float BatteryMonitor::readBatteryVoltage(int analogValue) const {
    // Exact proven formula: adcRawToBatteryVoltage + calibration offset
    return adcRawToBatteryVoltage(analogValue) + _calibrationOffset;
}

float BatteryMonitor::readVoltage() const {
    _cachedVoltage = readBatteryVoltage(readAnalogRawAveraged());
    _cachedVoltageMs = millis();
    return _cachedVoltage;
}

float BatteryMonitor::readVoltageCached(uint32_t maxAgeMs) const {
    if (_cachedVoltageMs != 0 && (millis() - _cachedVoltageMs) < maxAgeMs) {
        return _cachedVoltage;
    }
    return readVoltage();
}

float BatteryMonitor::readA0PinVoltage() const {
    return adcRawToA0PinVoltage(readAnalogRawAveraged());
}

float BatteryMonitor::getCellVoltage(float packVoltage) const {
    return packVoltage / (float)_cellsInSeries;
}

uint8_t BatteryMonitor::getStateOfCharge(float packVoltage) const {
    float cell = getCellVoltage(packVoltage);
    float soc = (cell - BATTERY_CELL_VOLTAGE_EMPTY) /
                (BATTERY_CELL_VOLTAGE_FULL - BATTERY_CELL_VOLTAGE_EMPTY);
    soc = constrain(soc, 0.0f, 1.0f);
    return (uint8_t)(soc * 100.0f);
}

BatteryState BatteryMonitor::getState(float packVoltage) const {
    if (packVoltage >= _thresholdLow) return BatteryState::GOOD;
    if (packVoltage >= _thresholdCritical) return BatteryState::LOW_BATT;
    return BatteryState::CRITICAL;
}

const char* BatteryMonitor::stateToString(BatteryState s) {
    switch (s) {
        case BatteryState::GOOD:     return "good";
        case BatteryState::LOW_BATT: return "low";
        case BatteryState::CRITICAL: return "critical";
        default:                     return "unknown";
    }
}

bool BatteryMonitor::isCritical(float packVoltage) const {
    return packVoltage < _thresholdCritical;
}

bool BatteryMonitor::needsRecharge(float packVoltage) const {
    return packVoltage < _thresholdRecharge;
}

String BatteryMonitor::getStatusString(float packVoltage) const {
    BatteryState st = getState(packVoltage);
    switch (st) {
        case BatteryState::GOOD:     return "GOOD";
        case BatteryState::LOW_BATT: return "LOW — recharge soon";
        case BatteryState::CRITICAL: return "CRITICAL — conserve power";
        default:                     return "UNKNOWN";
    }
}

String BatteryMonitor::getJson() const {
    const int raw = readAnalogRawAveraged();
    const float v = readBatteryVoltage(raw);
    const uint8_t pct = getStateOfCharge(v);
    const BatteryState st = getState(v);

    JsonDocument doc;
    doc["voltage"] = roundf(v * 1000.0f) / 1000.0f;
    doc["cellVoltage"] = roundf(getCellVoltage(v) * 1000.0f) / 1000.0f;
    doc["percent"] = pct;
    doc["state"] = stateToString(st);
    doc["status"] = getStatusString(v);
    doc["cells"] = _cellsInSeries;
    doc["chemistry"] = "Li-ion";
    doc["calibrationOffset"] = roundf(_calibrationOffset * 1000.0f) / 1000.0f;
    doc["adcRaw"] = raw;
    doc["a0PinV"] = roundf(adcRawToA0PinVoltage(raw) * 1000.0f) / 1000.0f;
    doc["scale"] = adcBatteryVoltageRatio();
    doc["rSeries"] = R_SERIES;
    doc["maxMeasurableV"] = roundf(adcMaxBatteryVoltage() * 100.0f) / 100.0f;

    String out;
    serializeJson(doc, out);
    return out;
}
