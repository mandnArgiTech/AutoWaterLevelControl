/**
 * @file UltrasonicSensor.h
 * @brief US-100 Ultrasonic sensor driver (FilteredSensorBase)
 * 
 * Distance measurement: 20–4500 mm via serial (UART) mode.
 * Jumper must be installed on the sensor for UART mode.
 * Communication: 9600 baud, command-response protocol.
 * 
 * @author FluidLevelMonitor Project
 * @version 1.1.0
 */

#ifndef ULTRASONIC_SENSOR_H
#define ULTRASONIC_SENSOR_H

#include <SoftwareSerial.h>
#include "FilteredSensorBase.h"

#define US100_CMD_DISTANCE  0x55
#define US100_CMD_TEMP      0x50
#define US100_BAUD_RATE     9600
#define US100_TIMEOUT_MS    200
#define US100_MIN_DISTANCE  20      // mm
#define US100_MAX_DISTANCE  4500    // mm
#define US100_TEMP_OFFSET   45

#define DEFAULT_US100_RX_PIN D1
#define DEFAULT_US100_TX_PIN D2

class UltrasonicSensor : public FilteredSensorBase {
public:
    UltrasonicSensor(int rxPin = DEFAULT_US100_RX_PIN,
                     int txPin = DEFAULT_US100_TX_PIN);
    ~UltrasonicSensor() override;

    // --- FilteredSensorBase / ISensor ---
    ErrorCode begin() override;
    float readRawDistanceMm() override;
    float readTemperature() override;
    SensorType getSensorType() const override { return SensorType::ULTRASONIC_US100; }
    String getSensorTypeName() const override { return F("US-100 Ultrasonic"); }
    float getMinRange() const override { return US100_MIN_DISTANCE; }
    float getMaxRange() const override { return US100_MAX_DISTANCE; }
    bool testConnection() override;
    String getStatusJson() const override;

    // --- US-100 specific ---
    void clearBuffer();
    void setTimeout(uint16_t ms) { _timeout = ms; }
    uint16_t getTimeout() const { return _timeout; }

protected:
    uint16_t getSampleDelay() const override { return 25; }

private:
    void sendCommand(uint8_t cmd);
    bool waitForBytes(uint8_t count);
    bool validateDistance(float distance);

    SoftwareSerial* _serial;
    int _rxPin;
    int _txPin;
    uint16_t _timeout;
};

#endif // ULTRASONIC_SENSOR_H
