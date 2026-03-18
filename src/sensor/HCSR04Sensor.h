/**
 * @file HCSR04Sensor.h
 * @brief HC-SR04 Ultrasonic Distance Sensor Driver (FilteredSensorBase)
 * 
 * Range: 20–4000 mm via trigger/echo pulse timing.
 * Needs 5 V power; ECHO pin requires a voltage divider for 3.3 V MCUs.
 * 
 * @author FluidLevelMonitor Project
 * @version 1.1.0
 */

#ifndef HCSR04_SENSOR_H
#define HCSR04_SENSOR_H

#include "FilteredSensorBase.h"

#define HCSR04_TRIGGER_PULSE_US     10
#define HCSR04_ECHO_TIMEOUT_US      30000   // ~5 m max
#define HCSR04_MIN_DISTANCE_MM      20
#define HCSR04_MAX_DISTANCE_MM      4000
#define HCSR04_SETTLING_TIME_MS     60

class HCSR04Sensor : public FilteredSensorBase {
public:
    HCSR04Sensor(int trigPin, int echoPin);
    ~HCSR04Sensor() override = default;

    // --- FilteredSensorBase / ISensor ---
    ErrorCode begin() override;
    float readRawDistanceMm() override;
    float readTemperature() override { return _temperature; }
    SensorType getSensorType() const override { return SensorType::ULTRASONIC_HCSR04; }
    String getSensorTypeName() const override { return F("HC-SR04"); }
    float getMinRange() const override { return HCSR04_MIN_DISTANCE_MM; }
    float getMaxRange() const override { return HCSR04_MAX_DISTANCE_MM; }
    bool testConnection() override;
    String getStatusJson() const override;

    void setTemperature(float tempC) { _temperature = tempC; }

protected:
    uint16_t getSampleDelay() const override { return 10; }

private:
    unsigned long measureEchoPulse();
    float getSpeedOfSound() const;
    bool validateDistance(float distance);

    int _trigPin;
    int _echoPin;
    float _temperature;
    unsigned long _lastMeasureTime;
};

#endif // HCSR04_SENSOR_H
