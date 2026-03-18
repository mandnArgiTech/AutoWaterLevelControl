/**
 * @file SmsMotorController.cpp
 * @brief SIM800L/A6 AT-command SMS implementation
 */
#include "SmsMotorController.h"

SmsMotorController::~SmsMotorController() {
    delete _gsm;
}

// =============================================================================
// SECTION 1: INIT
// =============================================================================

ErrorCode SmsMotorController::begin(const MotorConfig& config) {
    _config = config;
    if (!config.enabled) {
        _state = MotorState::DISABLED;
        FLM_LOG_INFO("SmsMotor", "disabled");
        return ErrorCode::ERR_NONE;
    }
    if (_config.maxRunMinutes > MOTOR_MAX_RUN_MINUTES)
        _config.maxRunMinutes = MOTOR_MAX_RUN_MINUTES;
    if (_config.targetPhone.isEmpty() || _config.onMessage.isEmpty()) {
        FLM_LOG_WARN("SmsMotor", "targetPhone or onMessage not configured");
    }

    _gsm = new SoftwareSerial(_config.gsmRxPin, _config.gsmTxPin);
    _gsm->begin(_config.gsmBaudRate > 0 ? _config.gsmBaudRate : 9600);
    delay(1000);

    if (!initGsm()) {
        FLM_LOG_WARN("SmsMotor", "GSM init failed — will retry in loop");
    }

    _mode  = MotorMode::AUTO;
    _state = MotorState::STOPPED;
    _initialized = true;
    FLM_LOG_INFO("SmsMotor", "ready phone=%s on='%s' off='%s'",
                 _config.targetPhone.c_str(),
                 _config.onMessage.c_str(),
                 _config.offMessage.c_str());
    return ErrorCode::ERR_NONE;
}

bool SmsMotorController::initGsm() {
    flushGsm();
    _gsm->println("AT");
    if (!waitForResponse("OK", 3000)) {
        FLM_LOG_WARN("SmsMotor", "GSM not responding");
        return false;
    }
    _gsm->println("AT+CMGF=1");    // Text mode SMS
    if (!waitForResponse("OK", 2000)) {
        FLM_LOG_WARN("SmsMotor", "AT+CMGF=1 failed");
        return false;
    }
    FLM_LOG_INFO("SmsMotor", "GSM ready");
    return true;
}

// =============================================================================
// SECTION 2: LOOP
// =============================================================================

void SmsMotorController::loop(float percentFilled) {
    if (!_initialized || _state == MotorState::DISABLED) return;
    _lastPct = percentFilled;

    // Resolve PENDING timeout
    if (_state == MotorState::PENDING) {
        uint32_t timeout = _config.confirmTimeoutMs > 0
                           ? _config.confirmTimeoutMs
                           : 30000;
        if (millis() - _pendingSince > timeout) {
            FLM_LOG_WARN("SmsMotor", "SMS confirmation timeout");
            _lastSmsError = "timeout";
            _state = MotorState::STOPPED;
        }
        return;
    }

    if (checkDryRun(percentFilled)) return;
    if (_state == MotorState::RUNNING) checkMaxRuntime();
    if (_state == MotorState::RUNNING && _mode == MotorMode::OFF)  commandOff();
    if (_state == MotorState::STOPPED && _mode == MotorMode::ON)   commandOn();
    if (_mode == MotorMode::AUTO) {
        if (_state == MotorState::STOPPED && percentFilled < _config.pumpOnPercent)   commandOn();
        if (_state == MotorState::RUNNING && percentFilled >= _config.pumpOffPercent) commandOff();
    }
}

void SmsMotorController::setMode(MotorMode mode) {
    if (_mode == mode) return;
    _mode = mode;
    if (mode == MotorMode::OFF && _state == MotorState::RUNNING) commandOff();
}

uint32_t SmsMotorController::getRunSeconds() const {
    if (_state != MotorState::RUNNING || _startedAt == 0) return 0;
    return (millis() - _startedAt) / 1000UL;
}

// =============================================================================
// SECTION 3: SMS COMMANDS
// =============================================================================

void SmsMotorController::commandOn() {
    FLM_LOG_INFO("SmsMotor", "sending ON SMS: '%s' → %s",
                 _config.onMessage.c_str(), _config.targetPhone.c_str());
    _state = MotorState::PENDING;
    _pendingSince = millis();

    if (sendSms(_config.onMessage)) {
        _state = MotorState::RUNNING;
        _startedAt = millis();
        _lastSmsSent = true;
        _lastSmsError = "";
        FLM_LOG_INFO("SmsMotor", "ON SMS sent OK");
    } else {
        _state = MotorState::STOPPED;
        _lastSmsSent = false;
        FLM_LOG_WARN("SmsMotor", "ON SMS failed: %s", _lastSmsError.c_str());
    }
}

void SmsMotorController::commandOff() {
    FLM_LOG_INFO("SmsMotor", "sending OFF SMS: '%s' → %s",
                 _config.offMessage.c_str(), _config.targetPhone.c_str());
    if (sendSms(_config.offMessage)) {
        _lastSmsSent = true;
        _lastSmsError = "";
        FLM_LOG_INFO("SmsMotor", "OFF SMS sent OK ran %us", getRunSeconds());
    } else {
        _lastSmsSent = false;
        FLM_LOG_WARN("SmsMotor", "OFF SMS failed: %s", _lastSmsError.c_str());
    }
    // Treat motor as stopped regardless of SMS result — fail-safe
    _state = MotorState::STOPPED;
    _startedAt = 0;
}

// =============================================================================
// SECTION 4: AT COMMANDS
// =============================================================================

bool SmsMotorController::sendSms(const String& message) {
    for (int attempt = 0; attempt <= GSM_SEND_RETRIES; attempt++) {
        if (attempt > 0) {
            FLM_LOG_INFO("SmsMotor", "retry %d", attempt);
            delay(GSM_INTER_RETRY_MS);
            yield();
        }
        flushGsm();

        // Set recipient
        _gsm->print("AT+CMGS=\"");
        _gsm->print(_config.targetPhone);
        _gsm->println("\"");

        if (!waitForResponse(">", 5000)) {
            _lastSmsError = "no prompt";
            continue;
        }

        // Send message body + Ctrl-Z
        _gsm->print(message);
        _gsm->write(26);  // Ctrl+Z

        if (waitForResponse("+CMGS:", 15000)) {
            return true;
        }
        _lastSmsError = "no CMGS ack";
    }
    return false;
}

bool SmsMotorController::waitForResponse(const String& expected, uint32_t timeoutMs) {
    String buffer;
    unsigned long start = millis();
    while (millis() - start < timeoutMs) {
        while (_gsm->available()) {
            char c = _gsm->read();
            buffer += c;
            if (buffer.indexOf(expected) >= 0) return true;
            if (buffer.length() > 200) buffer = buffer.substring(100);
        }
        yield();
        delay(10);
    }
    return false;
}

void SmsMotorController::flushGsm() {
    while (_gsm->available()) _gsm->read();
}

// =============================================================================
// SECTION 5: SAFETY
// =============================================================================

bool SmsMotorController::checkDryRun(float pct) {
    bool blocked = pct < MOTOR_DRY_RUN_GUARD_PCT;
    if (blocked && !_dryRunBlocked) {
        if (_state == MotorState::RUNNING) commandOff();
        FLM_LOG_WARN("SmsMotor", "dry-run guard %.1f%%", pct);
    }
    _dryRunBlocked = blocked;
    return blocked;
}

void SmsMotorController::checkMaxRuntime() {
    if ((millis() - _startedAt) >= (uint32_t)_config.maxRunMinutes * 60000UL) {
        FLM_LOG_WARN("SmsMotor", "max runtime exceeded — sending OFF");
        commandOff();
        _mode = MotorMode::OFF;
    }
}

// =============================================================================
// SECTION 6: JSON
// =============================================================================

String SmsMotorController::getStatusJson() const {
    JsonDocument doc;
    doc["controlType"]   = "sms";
    doc["enabled"]       = _config.enabled;
    doc["mode"]          = modeToString(_mode);
    doc["state"]         = stateToString(_state);
    doc["runSeconds"]    = getRunSeconds();
    doc["autoEnabled"]   = (_mode == MotorMode::AUTO);
    doc["dryRunBlocked"] = _dryRunBlocked;
    doc["levelPercent"]  = _lastPct;
    doc["lastSmsSent"]   = _lastSmsSent;
    if (!_lastSmsError.isEmpty()) doc["lastSmsError"] = _lastSmsError;
    JsonObject gsm = doc["gsm"].to<JsonObject>();
    gsm["targetPhone"]  = _config.targetPhone;
    gsm["onMessage"]    = _config.onMessage;
    gsm["offMessage"]   = _config.offMessage;
    JsonObject t = doc["thresholds"].to<JsonObject>();
    t["onPercent"]     = _config.pumpOnPercent;
    t["offPercent"]    = _config.pumpOffPercent;
    t["maxRunMinutes"] = _config.maxRunMinutes;
    t["dryRunGuardPct"]= MOTOR_DRY_RUN_GUARD_PCT;
    String out; serializeJson(doc, out); return out;
}

String SmsMotorController::getMQTTJson() const {
    JsonDocument doc;
    doc["controlType"] = "sms";
    doc["state"]       = stateToString(_state);
    doc["mode"]        = modeToString(_mode);
    doc["runSeconds"]  = getRunSeconds();
    doc["blocked"]     = _dryRunBlocked;
    doc["smsSent"]     = _lastSmsSent;
    if (!_lastSmsError.isEmpty()) doc["smsError"] = _lastSmsError;
    String out; serializeJson(doc, out); return out;
}
