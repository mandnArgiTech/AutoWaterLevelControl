/**
 * @file adc_scaling.h
 * @brief ADC voltage scaling for D1 Mini A0 wiring
 *
 * Copied from proven ADCA1115Calibration project.
 *
 * Circuit:
 *   Battery (+) --[R_SERIES]-- A0 (D1 Mini screw terminal)
 *   Battery (-) ---------------- GND
 *
 * The D1 Mini has an onboard divider on A0 (default 220k + 100k) that maps
 * 0–3.2 V at the A0 pin to 0–1.0 V at the ESP8266 ADC input.
 *
 * For a single Li-ion cell (~4.2 V max), R_SERIES = 100 kΩ (proven default).
 */

#ifndef ADC_SCALING_H
#define ADC_SCALING_H

#include <Arduino.h>

#ifndef VREF
#define VREF 1.0f
#endif

#ifndef R_SERIES
#define R_SERIES 100000.0f
#endif

#ifndef D1_A0_DIVIDER_HIGH
#define D1_A0_DIVIDER_HIGH 220000.0f
#endif

#ifndef D1_A0_DIVIDER_LOW
#define D1_A0_DIVIDER_LOW 100000.0f
#endif

/** Total resistance from A0 pin to GND through the onboard divider */
#define A0_TO_GND_OHMS (D1_A0_DIVIDER_HIGH + D1_A0_DIVIDER_LOW)

/** Max safe voltage at the A0 screw terminal on a D1 Mini */
#define A0_PIN_MAX_VOLTS 3.2f

static inline float adcRawToChipVoltage(int adcValue) {
    return (adcValue / 1023.0f) * VREF;
}

/** Voltage at the D1 Mini A0 screw terminal */
static inline float adcRawToA0PinVoltage(int adcValue) {
    const float vChip = adcRawToChipVoltage(adcValue);
    return vChip * A0_TO_GND_OHMS / D1_A0_DIVIDER_LOW;
}

/** Battery voltage for the series-resistor + onboard-divider circuit */
static inline float adcRawToBatteryVoltage(int adcValue) {
    const float vChip = adcRawToChipVoltage(adcValue);
    return vChip * (R_SERIES + A0_TO_GND_OHMS) / D1_A0_DIVIDER_LOW;
}

/** Effective multiplier from ADC chip voltage to battery voltage */
static inline float adcBatteryVoltageRatio() {
    return (R_SERIES + A0_TO_GND_OHMS) / D1_A0_DIVIDER_LOW;
}

/** Maximum measurable battery voltage before the A0 pin hits 3.2 V */
static inline float adcMaxBatteryVoltage() {
    return A0_PIN_MAX_VOLTS * (R_SERIES + A0_TO_GND_OHMS) / A0_TO_GND_OHMS;
}

#endif // ADC_SCALING_H
