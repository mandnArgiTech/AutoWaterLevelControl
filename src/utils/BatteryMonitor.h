/**
 * @file BatteryMonitor.h
 * @brief Single-cell battery monitor via D1 Mini A0 (from ADCA1115Calibration)
 *
 * Circuit (proven):
 *   Battery (+) --[R_SERIES=100k]-- A0
 *   Battery (-) -------------------- GND
 *   + D1 Mini onboard 220k/100k divider into ESP8266 ADC (0–1 V)
 *
 * This project measures ONE lithium-ion cell only (never a multi-cell pack).
 * Calibration: offset = actual_multimeter_V − measured_V
 */

#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Single-cell only (matches ADCA1115Calibration)
#ifndef BATTERY_CELLS_IN_SERIES
#define BATTERY_CELLS_IN_SERIES 1
#endif

#ifndef BATT_SAMPLES
#define BATT_SAMPLES 8
#endif

// Single Li-ion cell thresholds
#ifndef BATTERY_CELL_VOLTAGE_FULL
#define BATTERY_CELL_VOLTAGE_FULL     4.20f
#endif
#ifndef BATTERY_CELL_VOLTAGE_NOMINAL
#define BATTERY_CELL_VOLTAGE_NOMINAL  3.70f
#endif
#ifndef BATTERY_CELL_VOLTAGE_LOW
#define BATTERY_CELL_VOLTAGE_LOW      3.30f
#endif
#ifndef BATTERY_CELL_VOLTAGE_EMPTY
#define BATTERY_CELL_VOLTAGE_EMPTY    3.00f
#endif
#ifndef BATTERY_CELL_VOLTAGE_RECHARGE
#define BATTERY_CELL_VOLTAGE_RECHARGE 3.20f
#endif

// Compatibility aliases used elsewhere
#ifndef BATT_VOLTAGE_FULL
#define BATT_VOLTAGE_FULL     BATTERY_CELL_VOLTAGE_FULL
#endif
#ifndef BATT_VOLTAGE_NOMINAL
#define BATT_VOLTAGE_NOMINAL  BATTERY_CELL_VOLTAGE_NOMINAL
#endif
#ifndef BATT_VOLTAGE_LOW
#define BATT_VOLTAGE_LOW      BATTERY_CELL_VOLTAGE_LOW
#endif
#ifndef BATT_VOLTAGE_CRITICAL
#define BATT_VOLTAGE_CRITICAL BATTERY_CELL_VOLTAGE_EMPTY
#endif
#ifndef BATT_VOLTAGE_CUTOFF
#define BATT_VOLTAGE_CUTOFF   BATTERY_CELL_VOLTAGE_EMPTY
#endif

enum class BatteryState : uint8_t {
    GOOD     = 0,
    LOW_BATT = 1,
    CRITICAL = 2,
    UNKNOWN  = 3,
};

class BatteryMonitor {
public:
    static BatteryMonitor& getInstance();

    /** Always single-cell; cellsInSeries argument is ignored (proven behaviour). */
    void begin(uint8_t cellsInSeries = BATTERY_CELLS_IN_SERIES);
    void setCalibrationOffset(float offset);
    float getCalibrationOffset() const { return _calibrationOffset; }

    /** offset = actual − measured (same as ADCA1115Calibration) */
    void calibrate(float measuredActual, float measuredReading);

    int readAnalogRawAveraged(uint8_t samples = BATT_SAMPLES) const;
    float readBatteryVoltage(int analogValue) const;
    float readVoltage() const;
    /** Reuse last reading if younger than maxAgeMs (web poll friendly). */
    float readVoltageCached(uint32_t maxAgeMs = 3000) const;
    float readA0PinVoltage() const;

    float getCellVoltage(float packVoltage) const;
    uint8_t getStateOfCharge(float packVoltage) const;
    BatteryState getState(float packVoltage) const;
    static const char* stateToString(BatteryState s);
    bool isCritical(float packVoltage) const;
    bool needsRecharge(float packVoltage) const;
    String getStatusString(float packVoltage) const;

    String getJson() const;

    static float readVoltageStatic() { return getInstance().readVoltage(); }
    static String getJsonStatic() { return getInstance().getJson(); }

private:
    BatteryMonitor();
    BatteryMonitor(const BatteryMonitor&) = delete;
    BatteryMonitor& operator=(const BatteryMonitor&) = delete;

    uint8_t _cellsInSeries;
    float _calibrationOffset;
    float _thresholdFull;
    float _thresholdLow;
    float _thresholdCritical;
    float _thresholdRecharge;

    mutable float _cachedVoltage;
    mutable unsigned long _cachedVoltageMs;
};

#endif // BATTERY_MONITOR_H
