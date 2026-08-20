/**
 * @file A02YYUWSensor.h
 * @brief DYP-A02YYUW ultrasonic sensor (UART, non-blocking 1-byte FSM)
 *
 * Ported from tmp/a02yyuw_sensor.* — integrated with FilteredSensorBase + TankCalculator.
 */
#ifndef A02YYUW_SENSOR_H
#define A02YYUW_SENSOR_H

#include <SoftwareSerial.h>
#include "FilteredSensorBase.h"

#define A02YYUW_HEADER           0xFF
#define A02YYUW_BAUD_RATE        9600
#define A02YYUW_MIN_DISTANCE     30      ///< mm blind zone (SEN0311 / DYP-A02YYUW: 3 cm)
#define A02YYUW_MAX_DISTANCE     4500    ///< mm max range (SEN0311: 450 cm)
#define A02YYUW_FRAME_STALE_MS   3000u
#define DEFAULT_A02YYUW_RX_PIN   14      ///< D5 / GPIO14 (sensor TX -> ESP RX)
#define DEFAULT_A02YYUW_TX_PIN   12      ///< D6 / GPIO12 (sensor RX <- ESP TX)

enum class A02UartState : uint8_t {
    WaitHeader = 0,
    GotHeader,
    GotDataH,
    GotDataL
};

class A02YYUWSensor : public FilteredSensorBase {
public:
    A02YYUWSensor(int rxPin = DEFAULT_A02YYUW_RX_PIN,
                  int txPin = DEFAULT_A02YYUW_TX_PIN);
    ~A02YYUWSensor() override;

    ErrorCode begin() override;
    float readRawDistanceMm() override;
    float readDistanceAverageMm(uint8_t samples = 5) override;

    void poll() override;

    SensorType getSensorType() const override { return SensorType::ULTRASONIC_A02YYUW; }
    String getSensorTypeName() const override { return F("A02YYUW Ultrasonic"); }
    float getMinRange() const override { return A02YYUW_MIN_DISTANCE; }
    float getMaxRange() const override { return A02YYUW_MAX_DISTANCE; }
    bool testConnection() override;
    String getStatusJson() const override;

protected:
    uint16_t getSampleDelay() const override { return 0; }

private:
    void processUartByte(uint8_t byte);
    bool applyFrame(uint16_t distRawMm);

    SoftwareSerial* _serial;
    int _rxPin;
    int _txPin;

    A02UartState _parseState;
    uint8_t _parseDataH;
    uint8_t _parseDataL;

    float _cachedDistanceMm;
    uint16_t _lastFrameRawMm;   ///< Last decoded frame (even if rejected)
    unsigned long _lastFrameMs;
    uint32_t _frameCount;
    uint32_t _checksumErrors;
    String _rangeStatus;
};

#endif
