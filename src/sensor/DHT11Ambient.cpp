/**
 * @file DHT11Ambient.cpp
 */
#include "DHT11Ambient.h"
#include "../utils/Log.h"
#include <ArduinoJson.h>

DHT11Ambient::DHT11Ambient(int pin)
    : _pin(pin)
    , _dht(pin, DHT11)
    , _temperatureC(0.0f)
    , _humidityPct(0.0f)
    , _readOk(false)
    , _initialized(false)
    , _lastReadMs(0)
    , _readIntervalMs(DEFAULT_DHT11_READ_INTERVAL)
    , _errorCount(0) {
}

ErrorCode DHT11Ambient::begin() {
    _dht.begin();
    _initialized = true;
    FLM_LOG_INFO("DHT11", "ambient sensor on GPIO %d", _pin);
    return ErrorCode::ERR_NONE;
}

bool DHT11Ambient::update() {
    if (!_initialized) return false;

    unsigned long now = millis();
    if (_lastReadMs != 0 && (now - _lastReadMs) < _readIntervalMs) {
        return false;
    }
    _lastReadMs = now;

    float h = _dht.readHumidity();
    float t = _dht.readTemperature();

    if (isnan(h) || isnan(t)) {
        _readOk = false;
        _errorCount++;
        return false;
    }

    _humidityPct = h;
    _temperatureC = t;
    _readOk = true;
    return true;
}

String DHT11Ambient::getStatusJson() const {
    JsonDocument doc;
    doc["type"] = "DHT11";
    doc["pin"] = _pin;
    doc["ready"] = _initialized;
    doc["readOk"] = _readOk;
    doc["errorCount"] = _errorCount;
    if (_readOk) {
        doc["temperatureC"] = _temperatureC;
        doc["humidityPct"] = _humidityPct;
    }
    String out;
    serializeJson(doc, out);
    return out;
}
