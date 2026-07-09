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
#define A02YYUW_MIN_DISTANCE     280     ///< mm blind zone (DYP-A02YYUW)
#define A02YYUW_MAX_DISTANCE     7500    ///< mm
#define A02YYUW_FRAME_STALE_MS   3000u
#define DEFAULT_A02YYUW_RX_PIN   5       ///< D1 / GPIO5
#define DEFAULT_A02YYUW_TX_PIN   4       ///< D2 / GPIO4

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
    unsigned long _lastFrameMs;
    uint32_t _frameCount;
    uint32_t _checksumErrors;
    String _rangeStatus;
};

#endif
