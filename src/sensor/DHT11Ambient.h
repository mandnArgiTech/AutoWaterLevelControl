/**
 * @file DHT11Ambient.h
 * @brief On-board DHT11 ambient temperature + humidity (tmp/dht11_sensor.*)
 */
#ifndef DHT11_AMBIENT_H
#define DHT11_AMBIENT_H

#include <Arduino.h>
#include <DHT.h>
#include "../utils/ErrorHandler.h"

#define DEFAULT_DHT11_PIN            2       ///< D4 / GPIO2
#define DEFAULT_DHT11_READ_INTERVAL  3000u   ///< ms — DHT11 min ~2s between reads

class DHT11Ambient {
public:
    explicit DHT11Ambient(int pin = DEFAULT_DHT11_PIN);

    ErrorCode begin();
    void setReadIntervalMs(uint32_t ms) { _readIntervalMs = ms; }
    /** Read if interval elapsed; returns true when a fresh sample was taken. */
    bool update();
    bool hasValidReading() const { return _readOk; }
    float getTemperatureC() const { return _temperatureC; }
    float getHumidityPct() const { return _humidityPct; }
    String getStatusJson() const;

private:
    int _pin;
    DHT _dht;
    float _temperatureC;
    float _humidityPct;
    bool _readOk;
    bool _initialized;
    unsigned long _lastReadMs;
    uint32_t _readIntervalMs;
    uint32_t _errorCount;
};

#endif
