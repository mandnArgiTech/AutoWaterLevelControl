/**
 * @file SmsMotorController.cpp
 * @brief Non-blocking SMS FSM; OFF failure keeps RUNNING + retry; GSM init retry in loop
 */
#include "SmsMotorController.h"
#include "../utils/ErrorHandler.h"

#define SMS_WAIT_PROMPT_MS   8000u
#define SMS_WAIT_CMGS_MS     20000u
#define SMS_RETRY_WAIT_MS    5000u
#define SMS_MAX_ATTEMPTS     3u

SmsMotorController::~SmsMotorController() {
    delete _gsm;
}

bool SmsMotorController::bufferHas(const String& buf, const char* needle) const {
    return buf.indexOf(needle) >= 0;
}

bool SmsMotorController::initGsmBlocking() {
    if (!_gsm) return false;
    flushGsm();
    _gsm->println(F("AT"));
    unsigned long t0 = millis();
    String acc;
    while (millis() - t0 < 4000) {
        ESP.wdtFeed();
        while (_gsm->available()) {
            char c = (char)_gsm->read();
            acc += c;
            if (acc.indexOf("OK") >= 0) goto ok1;
            if (acc.length() > 120) acc = acc.substring(60);
        }
        yield();
        delay(15);
    }
    return false;
ok1:
    flushGsm();
    _gsm->println(F("AT+CMGF=1"));
    t0 = millis();
    acc = "";
    while (millis() - t0 < 3000) {
        ESP.wdtFeed();
        while (_gsm->available()) {
            char c = (char)_gsm->read();
            acc += c;
            if (acc.indexOf("OK") >= 0) {
                FLM_LOG_INFO("SmsMotor", "GSM ready");
                return true;
            }
            if (acc.length() > 120) acc = acc.substring(60);
        }
        yield();
        delay(15);
    }
    return false;
}

ErrorCode SmsMotorController::begin(const MotorConfig& config) {
    _config = config;
    if (!config.enabled) {
        _state = MotorState::DISABLED;
        FLM_LOG_INFO("SmsMotor", "disabled");
        return ErrorCode::ERR_NONE;
    }
    if (_config.maxRunMinutes > MOTOR_MAX_RUN_MINUTES)
        _config.maxRunMinutes = MOTOR_MAX_RUN_MINUTES;

    if (_config.targetPhone.isEmpty() || _config.onMessage.isEmpty()
        || _config.offMessage.isEmpty()) {
        FLM_LOG_WARN("SmsMotor", "targetPhone/onMessage/offMessage must be set for safe operation");
        ErrorHandler::getInstance().logError(ErrorCode::ERR_CONFIG_VALIDATE,
            "SMS motor: configure targetPhone, onMessage, offMessage");
    }

    _gsm = new SoftwareSerial(_config.gsmRxPin, _config.gsmTxPin);
    _gsm->begin(_config.gsmBaudRate > 0 ? _config.gsmBaudRate : 9600);
    _gsmOk = false;
    _nextGsmInitMs = millis() + 800;

    _mode  = MotorMode::AUTO;
    _state = MotorState::STOPPED;
    _initialized = true;
    _smsPhase = SmsPhase::Idle;
    _smsJob = SmsJob::None;
    FLM_LOG_INFO("SmsMotor", "ready phone=%s", _config.targetPhone.c_str());
    return ErrorCode::ERR_NONE;
}

void SmsMotorController::tryGsmInit() {
    if (_gsmOk || !_gsm || millis() < _nextGsmInitMs) return;
    if (initGsmBlocking()) {
        _gsmOk = true;
    } else {
        FLM_LOG_WARN("SmsMotor", "GSM init failed — retry in %us", (unsigned)(GSM_INIT_RETRY_MS / 1000));
        _nextGsmInitMs = millis() + GSM_INIT_RETRY_MS;
    }
}

void SmsMotorController::flushGsm() {
    if (!_gsm) return;
    while (_gsm->available()) _gsm->read();
}

void SmsMotorController::startSmsJob(SmsJob job, const String& body) {
    if (!_gsm || !_gsmOk || _config.targetPhone.isEmpty()) {
        _lastSmsError = "gsm_not_ready";
        FLM_LOG_ERROR("SmsMotor", "SMS job aborted: GSM not ready or no phone");
        if (job == SmsJob::Off) {
            _state = MotorState::RUNNING;
            _pendingOffRetry = true;
            _nextOffRetryMs = millis() + GSM_OFF_RETRY_MS;
            ErrorHandler::getInstance().logError(ErrorCode::ERR_NONE,
                "OFF SMS failed (no GSM) — motor assumed RUNNING; retrying");
        } else {
            _state = MotorState::STOPPED;
            ErrorHandler::getInstance().logError(ErrorCode::ERR_NONE, "ON SMS failed (no GSM)");
        }
        return;
    }
    _smsJob = job;
    _smsBody = body;
    _smsAttempt = 0;
    _rxLine = "";
    flushGsm();
    _gsm->print(F("AT+CMGS=\""));
    _gsm->print(_config.targetPhone);
    _gsm->println(F("\""));
    _smsPhase = SmsPhase::WaitPrompt;
    _smsDeadline = millis() + SMS_WAIT_PROMPT_MS;
}

void SmsMotorController::onSmsSuccess() {
    SmsJob j = _smsJob;
    _smsJob = SmsJob::None;
    _smsPhase = SmsPhase::Idle;
    _lastSmsSent = true;
    _lastSmsError = "";
    if (j == SmsJob::On) {
        _state = MotorState::RUNNING;
        _startedAt = millis();
        FLM_LOG_INFO("SmsMotor", "ON SMS OK");
    } else if (j == SmsJob::Off) {
        uint32_t ran = getRunSeconds();
        _state = MotorState::STOPPED;
        _startedAt = 0;
        _pendingOffRetry = false;
        FLM_LOG_INFO("SmsMotor", "OFF SMS OK ran %us", (unsigned)ran);
    }
}

void SmsMotorController::onSmsFailure() {
    if (_smsPhase == SmsPhase::RetryWait) return;

    _smsAttempt++;
    if (_smsAttempt < SMS_MAX_ATTEMPTS) {
        _smsPhase = SmsPhase::RetryWait;
        _smsDeadline = millis() + SMS_RETRY_WAIT_MS;
        FLM_LOG_WARN("SmsMotor", "SMS fail — retry %u/%u", (unsigned)_smsAttempt,
                     (unsigned)SMS_MAX_ATTEMPTS);
        return;
    }

    SmsJob j = _smsJob;
    _smsJob = SmsJob::None;
    _smsPhase = SmsPhase::Idle;
    _lastSmsSent = false;
    _lastSmsError = "send_failed";

    if (j == SmsJob::On) {
        _state = MotorState::STOPPED;
        FLM_LOG_WARN("SmsMotor", "ON SMS failed");
        ErrorHandler::getInstance().logError(ErrorCode::ERR_NONE, "ON SMS failed");
    } else {
        _state = MotorState::RUNNING;
        _pendingOffRetry = true;
        _nextOffRetryMs = millis() + GSM_OFF_RETRY_MS;
        FLM_LOG_ERROR("SmsMotor", "OFF SMS failed — state remains RUNNING; will retry");
        ErrorHandler::getInstance().logError(ErrorCode::ERR_NONE,
            "OFF SMS failed — motor assumed still ON; retrying");
    }
}

void SmsMotorController::pollSmsFsm() {
    if (!_gsm || _smsPhase == SmsPhase::Idle) return;

    if (_smsPhase == SmsPhase::RetryWait) {
        if ((long)(millis() - _smsDeadline) >= 0) {
            _rxLine = "";
            flushGsm();
            _gsm->print(F("AT+CMGS=\""));
            _gsm->print(_config.targetPhone);
            _gsm->println(F("\""));
            _smsPhase = SmsPhase::WaitPrompt;
            _smsDeadline = millis() + SMS_WAIT_PROMPT_MS;
        }
        return;
    }

    while (_gsm->available()) {
        ESP.wdtFeed();
        char c = (char)_gsm->read();
        if (c == '\r') continue;
        _rxLine += c;
        if (_rxLine.length() > 300)
            _rxLine = _rxLine.substring(150);
    }

    if (_smsPhase == SmsPhase::WaitPrompt) {
        if (bufferHas(_rxLine, ">")) {
            _gsm->print(_smsBody);
            _gsm->write(26);
            _rxLine = "";
            _smsPhase = SmsPhase::WaitCmgs;
            _smsDeadline = millis() + SMS_WAIT_CMGS_MS;
        } else if ((long)(millis() - _smsDeadline) >= 0) {
            _lastSmsError = "no_prompt";
            onSmsFailure();
        }
        return;
    }

    if (_smsPhase == SmsPhase::WaitCmgs) {
        if (bufferHas(_rxLine, "+CMGS:")) {
            onSmsSuccess();
        } else if (bufferHas(_rxLine, "ERROR") || bufferHas(_rxLine, "+CMS ERROR")) {
            _lastSmsError = "modem_error";
            onSmsFailure();
        } else if ((long)(millis() - _smsDeadline) >= 0) {
            _lastSmsError = "no_cmgs";
            onSmsFailure();
        }
    }
}

void SmsMotorController::commandOn() {
    if (_config.targetPhone.isEmpty() || _config.onMessage.isEmpty()) {
        FLM_LOG_ERROR("SmsMotor", "ON blocked: missing phone or onMessage");
        ErrorHandler::getInstance().logError(ErrorCode::ERR_CONFIG_VALIDATE, "SMS ON not configured");
        return;
    }
    _state = MotorState::PENDING;
    _pendingSince = millis();
    startSmsJob(SmsJob::On, _config.onMessage);
}

void SmsMotorController::commandOff() {
    if (_config.targetPhone.isEmpty() || _config.offMessage.isEmpty()) {
        FLM_LOG_ERROR("SmsMotor", "OFF blocked: missing phone or offMessage");
        return;
    }
    if (_smsJob == SmsJob::Off && _smsPhase != SmsPhase::Idle) return;
    startSmsJob(SmsJob::Off, _config.offMessage);
}

void SmsMotorController::loop(float percentFilled) {
    if (!_initialized || _state == MotorState::DISABLED) return;

    tryGsmInit();
    pollSmsFsm();

    _lastPct = percentFilled;

    if (_state == MotorState::PENDING) {
        uint32_t timeout = _config.confirmTimeoutMs > 0 ? _config.confirmTimeoutMs : 30000;
        if (millis() - _pendingSince > timeout && _smsPhase == SmsPhase::Idle && _smsJob == SmsJob::None) {
            FLM_LOG_WARN("SmsMotor", "PENDING timeout");
            _lastSmsError = "timeout";
            _state = MotorState::STOPPED;
        }
    }

    if (_pendingOffRetry && _state == MotorState::RUNNING && _smsJob == SmsJob::None
        && _smsPhase == SmsPhase::Idle && (long)(millis() - _nextOffRetryMs) >= 0) {
        _pendingOffRetry = false;
        FLM_LOG_WARN("SmsMotor", "retry OFF SMS");
        commandOff();
    }

    const bool levelKnown = (percentFilled >= 0.f && percentFilled <= 100.f);
    if (!levelKnown) {
        if (_state == MotorState::RUNNING) checkMaxRuntime();
        if (_state == MotorState::RUNNING && _mode == MotorMode::OFF && _smsJob == SmsJob::None)
            commandOff();
        if (_state == MotorState::STOPPED && _mode == MotorMode::ON && _smsJob == SmsJob::None)
            commandOn();
        return;
    }

    if (checkDryRun(percentFilled)) return;
    if (_state == MotorState::RUNNING && _smsJob == SmsJob::None) checkMaxRuntime();
    if (_state == MotorState::RUNNING && _mode == MotorMode::OFF && _smsJob == SmsJob::None)
        commandOff();
    if (_state == MotorState::STOPPED && _mode == MotorMode::ON && _smsJob == SmsJob::None)
        commandOn();
    if (_mode == MotorMode::AUTO && _smsJob == SmsJob::None) {
        if (_state == MotorState::STOPPED && percentFilled < _config.pumpOnPercent) commandOn();
        if (_state == MotorState::RUNNING && percentFilled >= _config.pumpOffPercent) commandOff();
    }
}

void SmsMotorController::setMode(MotorMode mode) {
    if (_mode == mode) return;
    _mode = mode;
    if (mode == MotorMode::OFF && _state == MotorState::RUNNING && _smsJob == SmsJob::None)
        commandOff();
}

uint32_t SmsMotorController::getRunSeconds() const {
    if (_state != MotorState::RUNNING || _startedAt == 0) return 0;
    return (millis() - _startedAt) / 1000UL;
}

bool SmsMotorController::checkDryRun(float pct) {
    bool blocked = pct < MOTOR_DRY_RUN_GUARD_PCT;
    if (blocked && !_dryRunBlocked) {
        if (_state == MotorState::RUNNING && _smsJob == SmsJob::None) commandOff();
        FLM_LOG_WARN("SmsMotor", "dry-run guard %.1f%%", pct);
        ErrorHandler::getInstance().logError(ErrorCode::ERR_NONE, "SMS dry-run guard");
    }
    _dryRunBlocked = blocked;
    return blocked;
}

void SmsMotorController::checkMaxRuntime() {
    if (_smsJob != SmsJob::None) return;
    if ((millis() - _startedAt) >= (uint32_t)_config.maxRunMinutes * 60000UL) {
        FLM_LOG_WARN("SmsMotor", "max runtime — OFF SMS");
        commandOff();
        _mode = MotorMode::OFF;
    }
}

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
    doc["gsmOk"]         = _gsmOk;
    doc["lastSmsSent"]   = _lastSmsSent;
    if (!_lastSmsError.isEmpty()) doc["lastSmsError"] = _lastSmsError;
    JsonObject gsm = doc["gsm"].to<JsonObject>();
    gsm["targetPhone"]  = _config.targetPhone;
    gsm["onMessage"]    = _config.onMessage;
    gsm["offMessage"]   = _config.offMessage;
    JsonObject t = doc["thresholds"].to<JsonObject>();
    t["onPercent"]      = _config.pumpOnPercent;
    t["offPercent"]     = _config.pumpOffPercent;
    t["maxRunMinutes"]  = _config.maxRunMinutes;
    t["dryRunGuardPct"] = MOTOR_DRY_RUN_GUARD_PCT;
    String out;
    serializeJson(doc, out);
    return out;
}

String SmsMotorController::getMQTTJson() const {
    JsonDocument doc;
    doc["controlType"] = "sms";
    doc["state"]       = stateToString(_state);
    doc["mode"]        = modeToString(_mode);
    doc["runSeconds"]  = getRunSeconds();
    doc["blocked"]     = _dryRunBlocked;
    doc["smsSent"]     = _lastSmsSent;
    doc["gsmOk"]       = _gsmOk;
    if (!_lastSmsError.isEmpty()) doc["smsError"] = _lastSmsError;
    String out;
    serializeJson(doc, out);
    return out;
}
