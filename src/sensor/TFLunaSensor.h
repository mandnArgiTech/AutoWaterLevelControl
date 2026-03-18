/**
 * @file TFLunaSensor.h
 * @brief Benewake TF-Luna LiDAR Distance Sensor Driver (FilteredSensorBase)
 * 
 * Range: 200–8000 mm, resolution 1 cm, UART 115200 baud.
 * 9-byte frame: header(2) + distance(2) + strength(2) + temp(2) + checksum(1)
 * 
 * @author FluidLevelMonitor Project
 * @version 1.1.0
 */

#ifndef TFLUNA_SENSOR_H
#define TFLUNA_SENSOR_H

#include "FilteredSensorBase.h"
#include <SoftwareSerial.h>

#define TFLUNA_BAUD_RATE        115200
#define TFLUNA_FRAME_SIZE       9
#define TFLUNA_HEADER           0x59
#define TFLUNA_MIN_DISTANCE_MM  200
#define TFLUNA_MAX_DISTANCE_MM  8000
#define TFLUNA_TIMEOUT_MS       100
#define TFLUNA_MIN_STRENGTH     100

class TFLunaSensor : public FilteredSensorBase {
public:
    TFLunaSensor(int rxPin, int txPin);
    ~TFLunaSensor() override;

    // --- FilteredSensorBase / ISensor ---
    ErrorCode begin() override;
    float readRawDistanceMm() override;
    float readTemperature() override { return _temperature; }
    SensorType getSensorType() const override { return SensorType::LASER_LIDAR; }
    String getSensorTypeName() const override { return F("TF-Luna"); }
    float getMinRange() const override { return TFLUNA_MIN_DISTANCE_MM; }
    float getMaxRange() const override { return TFLUNA_MAX_DISTANCE_MM; }
    bool testConnection() override;
    String getStatusJson() const override;

    uint16_t getSignalStrength() const { return _signalStrength; }
    bool setFrameRate(uint16_t fps);

protected:
    uint16_t getSampleDelay() const override { return 10; }

private:
    bool readFrame();
    bool parseFrame(uint8_t* frame);
    bool validateChecksum(uint8_t* frame);
    void clearBuffer();

    SoftwareSerial* _serial;
    int _rxPin;
    int _txPin;
    float _temperature;
    uint16_t _signalStrength;
    uint16_t _rawDistance;
    uint32_t _checksumErrors;
};

#endif // TFLUNA_SENSOR_H
