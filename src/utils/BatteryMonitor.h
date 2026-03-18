/**
 * @file BatteryMonitor.h
 * @brief Battery voltage monitoring for 2S LiFePO4 pack (2× 32700)
 *
 * Hardware:
 *   D1 Mini A0 pin ← resistor divider from battery pack
 *   100kΩ from Vm+ to A0, 24kΩ from A0 to GND
 *   Scale factor: (100 + 24) / 24 = 5.1667
 *
 * Pack voltage range:
 *   5.0V  = 0%   (BMS discharge cutoff)
 *   6.4V  = ~60% (nominal)
 *   7.3V  = 100% (fully charged, 2× 3.65V)
 *
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Resistor divider (1% tolerance)
#define BATT_R_HIGH_KOHM        100.0f   // 100kΩ high-side
#define BATT_R_LOW_KOHM          24.0f   // 24kΩ low-side
#define BATT_SCALE  ((BATT_R_HIGH_KOHM + BATT_R_LOW_KOHM) / BATT_R_LOW_KOHM)  // 5.1667

// ADC reference (D1 Mini: 1.0V full scale on A0)
#define BATT_ADC_REF_V           1.0f
#define BATT_ADC_MAX             1023.0f

// 2S LiFePO4 voltage thresholds
#define BATT_VOLTAGE_FULL        7.30f   // 2× 3.65V charged
#define BATT_VOLTAGE_NOMINAL     6.40f   // 2× 3.20V nominal
#define BATT_VOLTAGE_LOW         5.80f   // ~35% — warn
#define BATT_VOLTAGE_CRITICAL    5.30f   // ~13% — stop publishing, conserve
#define BATT_VOLTAGE_CUTOFF      5.00f   // 0%  — BMS disconnects

// Smoothing: average N ADC readings
#define BATT_SAMPLES             8

enum class BatteryState : uint8_t {
    GOOD     = 0,   ///< > 5.8V (~35%+)
    LOW      = 1,   ///< 5.3V–5.8V — warn, increase publish interval
    CRITICAL = 2,   ///< < 5.3V — publish alert only, deep sleep longer
    UNKNOWN  = 3,   ///< ADC not calibrated / not read yet
};

class BatteryMonitor {
public:
    /**
     * @brief Read and return smoothed battery voltage in volts
     */
    static float readVoltage() {
        uint32_t sum = 0;
        for (uint8_t i = 0; i < BATT_SAMPLES; i++) {
            sum += analogRead(A0);
            if (i < BATT_SAMPLES - 1) delay(2);
        }
        float adcAvg = sum / (float)BATT_SAMPLES;
        float voltage = (adcAvg / BATT_ADC_MAX) * BATT_ADC_REF_V * BATT_SCALE;
        return voltage;
    }

    /**
     * @brief Convert voltage to state-of-charge percentage (0–100)
     * LiFePO4 has a very flat discharge curve — linear approximation is adequate.
     */
    static uint8_t voltageToPercent(float v) {
        if (v >= BATT_VOLTAGE_FULL)   return 100;
        if (v <= BATT_VOLTAGE_CUTOFF) return 0;
        float pct = (v - BATT_VOLTAGE_CUTOFF) / (BATT_VOLTAGE_FULL - BATT_VOLTAGE_CUTOFF) * 100.0f;
        return (uint8_t)constrain(pct, 0, 100);
    }

    /**
     * @brief Get battery health state
     */
    static BatteryState getState(float v) {
        if (v >= BATT_VOLTAGE_LOW)      return BatteryState::GOOD;
        if (v >= BATT_VOLTAGE_CRITICAL) return BatteryState::LOW;
        return BatteryState::CRITICAL;
    }

    static const char* stateToString(BatteryState s) {
        switch (s) {
            case BatteryState::GOOD:     return "good";
            case BatteryState::LOW:      return "low";
            case BatteryState::CRITICAL: return "critical";
            default:                     return "unknown";
        }
    }

    /**
     * @brief Get battery status as JSON object string
     * Suitable for embedding in MQTT payload.
     */
    static String getJson() {
        float v   = readVoltage();
        uint8_t pct = voltageToPercent(v);
        BatteryState st = getState(v);

        JsonDocument doc;
        doc["voltage"] = roundf(v * 100.0f) / 100.0f;  // 2 decimal places
        doc["percent"] = pct;
        doc["state"]   = stateToString(st);
        doc["cells"]   = 2;
        doc["chemistry"] = "LiFePO4";

        String out;
        serializeJson(doc, out);
        return out;
    }
};

#endif // BATTERY_MONITOR_H
