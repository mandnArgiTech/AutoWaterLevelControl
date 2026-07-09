/**
 * @file A02YYUWSensor.cpp
 * @brief DYP-A02YYUW — non-blocking UART parser + FilteredSensorBase integration
 */
#include "A02YYUWSensor.h"
#include "../utils/Log.h"

A02YYUWSensor::A02YYUWSensor(int rxPin, int txPin)
    : _serial(nullptr)
    , _rxPin(rxPin)
    , _txPin(txPin)
    , _parseState(A02UartState::WaitHeader)
    , _parseDataH(0)
    , _parseDataL(0)
    , _cachedDistanceMm(-1.0f)
    , _lastFrameMs(0)
    , _frameCount(0)
    , _checksumErrors(0)
    , _rangeStatus("no_frame") {
}

A02YYUWSensor::~A02YYUWSensor() {
    if (_serial) {
        _serial->end();
        delete _serial;
        _serial = nullptr;
    }
}

ErrorCode A02YYUWSensor::begin() {
    FLM_LOG_INFO("A02YYUW", "Initializing...");

    if (_serial) {
        _serial->end();
        delete _serial;
    }
    _serial = new SoftwareSerial(_rxPin, _txPin);
    if (!_serial) {
        _lastError = ErrorCode::ERR_SYSTEM_MEMORY;
        return ErrorHandler::getInstance().logError(_lastError);
    }

    _serial->begin(A02YYUW_BAUD_RATE);
    _serial->listen();   // arm RX once; re-arming every poll() glitches other SoftwareSerial users
    delay(80);
    _parseState = A02UartState::WaitHeader;
    _cachedDistanceMm = -1.0f;
    _lastFrameMs = 0;

    _initialized = true;
    FLM_LOG_INFO("A02YYUW", "RX=%d TX=%d @ %u baud  range %d–%d mm",
                 _rxPin, _txPin, (unsigned)A02YYUW_BAUD_RATE,
                 A02YYUW_MIN_DISTANCE, A02YYUW_MAX_DISTANCE);
    return ErrorCode::ERR_NONE;
}

bool A02YYUWSensor::applyFrame(uint16_t distRawMm) {
    _lastFrameMs = millis();
    _frameCount++;

    if (distRawMm <= A02YYUW_MIN_DISTANCE || distRawMm >= A02YYUW_MAX_DISTANCE) {
        _rangeStatus = (distRawMm <= A02YYUW_MIN_DISTANCE) ? "below_blind" : "above_max";
        _lastError = ErrorCode::ERR_SENSOR_INVALID_DATA;
        _errorCount++;
        return false;
    }

    _cachedDistanceMm = static_cast<float>(distRawMm) + _calibrationOffset;
    _rangeStatus = "ok";
    _readCount++;
    _lastError = ErrorCode::ERR_NONE;
    return true;
}

void A02YYUWSensor::processUartByte(uint8_t byte) {
    switch (_parseState) {
        case A02UartState::WaitHeader:
            if (byte == A02YYUW_HEADER) {
                _parseState = A02UartState::GotHeader;
            }
            break;

        case A02UartState::GotHeader:
            _parseDataH = byte;
            _parseState = A02UartState::GotDataH;
            break;

        case A02UartState::GotDataH:
            _parseDataL = byte;
            _parseState = A02UartState::GotDataL;
            break;

        case A02UartState::GotDataL: {
            uint8_t calcSum = static_cast<uint8_t>(
                (A02YYUW_HEADER + _parseDataH + _parseDataL) & 0xFF);
            if (calcSum != byte) {
                _checksumErrors++;
            } else {
                uint16_t distRawMm =
                    (static_cast<uint16_t>(_parseDataH) << 8) | _parseDataL;
                applyFrame(distRawMm);
            }
            _parseState = A02UartState::WaitHeader;
            break;
        }

        default:
            _parseState = A02UartState::WaitHeader;
            break;
    }
}

void A02YYUWSensor::poll() {
    if (!_initialized || !_serial) return;

    while (_serial->available()) {
        processUartByte(static_cast<uint8_t>(_serial->read()));
    }
}

float A02YYUWSensor::readRawDistanceMm() {
    if (!_initialized) {
        _lastError = ErrorCode::ERR_SENSOR_INIT;
        return -1;
    }

    if (_frameCount == 0) {
        _rangeStatus = "no_frame";
        _lastError = ErrorCode::ERR_SENSOR_TIMEOUT;
        return -1;
    }

    if (millis() - _lastFrameMs > A02YYUW_FRAME_STALE_MS) {
        _rangeStatus = "stale";
        _lastError = ErrorCode::ERR_SENSOR_TIMEOUT;
        return -1;
    }

    if (_cachedDistanceMm < 0) {
        _lastError = ErrorCode::ERR_SENSOR_INVALID_DATA;
        return -1;
    }

    return _cachedDistanceMm;
}

float A02YYUWSensor::readDistanceAverageMm(uint8_t samples) {
    (void)samples;
    return readDistanceMm();
}

bool A02YYUWSensor::testConnection() {
    unsigned long start = millis();
    while (millis() - start < 2000) {
        poll();
        if (_frameCount > 0 && _rangeStatus == "ok") {
            return true;
        }
        yield();
        delay(10);
    }
    return _frameCount > 0;
}

String A02YYUWSensor::getStatusJson() const {
    JsonDocument doc;
    doc["type"] = "A02YYUW";
    doc["model"] = "DYP-A02YYUW";
    doc["interface"] = "UART";
    doc["rxPin"] = _rxPin;
    doc["txPin"] = _txPin;
    doc["baud"] = A02YYUW_BAUD_RATE;
    doc["rangeStatus"] = _rangeStatus;
    doc["frameCount"] = _frameCount;
    doc["checksumErrors"] = _checksumErrors;
    doc["frameAgeMs"] = (_lastFrameMs > 0) ? (millis() - _lastFrameMs) : 0;
    doc["ready"] = _initialized;

    addFilterStatusJson(doc);
    addReadingStatusJson(doc);

    String out;
    serializeJson(doc, out);
    return out;
}
