#include "RelayMotorController.h"

ErrorCode RelayMotorController::begin(const MotorConfig& config) {
    _config = config;
    if (!config.enabled) {
        _state = MotorState::DISABLED;
        FLM_LOG_INFO("RelayMotor", "disabled");
        return ErrorCode::ERR_NONE;
    }
    if (_config.maxRunMinutes > MOTOR_MAX_RUN_MINUTES)
        _config.maxRunMinutes = MOTOR_MAX_RUN_MINUTES;

    pinMode(_config.pin, OUTPUT);
    setRelay(false);
    _mode  = MotorMode::AUTO;
    _state = MotorState::STOPPED;
    _initialized = true;
    FLM_LOG_INFO("RelayMotor", "ready pin=%d activeLow=%d", _config.pin, _config.activeLow);
    return ErrorCode::ERR_NONE;
}

void RelayMotorController::loop(float percentFilled) {
    if (!_initialized || _state == MotorState::DISABLED) return;
    _lastPct = percentFilled;
    if (checkDryRun(percentFilled)) return;
    if (_state == MotorState::RUNNING) { checkMaxRuntime(); }
    if (_state == MotorState::RUNNING && _mode == MotorMode::OFF) stopMotor("manual_off");
    if (_state == MotorState::STOPPED && _mode == MotorMode::ON)  startMotor();
    if (_mode == MotorMode::AUTO) {
        if (_state == MotorState::STOPPED && percentFilled < _config.pumpOnPercent)  startMotor();
        if (_state == MotorState::RUNNING && percentFilled >= _config.pumpOffPercent) stopMotor("auto_threshold");
    }
}

void RelayMotorController::setMode(MotorMode mode) {
    if (_mode == mode) return;
    _mode = mode;
    if (mode == MotorMode::OFF && _state == MotorState::RUNNING) stopMotor("mode_off");
}

uint32_t RelayMotorController::getRunSeconds() const {
    if (_state != MotorState::RUNNING || _startedAt == 0) return 0;
    return (millis() - _startedAt) / 1000UL;
}

void RelayMotorController::startMotor() {
    if (_state == MotorState::RUNNING) return;
    setRelay(true);
    _state = MotorState::RUNNING;
    _startedAt = millis();
    FLM_LOG_INFO("RelayMotor", "ON");
}

void RelayMotorController::stopMotor(const char* reason) {
    if (_state == MotorState::STOPPED) return;
    setRelay(false);
    FLM_LOG_INFO("RelayMotor", "OFF [%s] ran %us", reason, getRunSeconds());
    _state = MotorState::STOPPED;
    _startedAt = 0;
}

void RelayMotorController::setRelay(bool on) {
    digitalWrite(_config.pin, (_config.activeLow ? !on : on) ? HIGH : LOW);
}

bool RelayMotorController::checkDryRun(float pct) {
    bool blocked = pct < MOTOR_DRY_RUN_GUARD_PCT;
    if (blocked && !_dryRunBlocked) {
        if (_state == MotorState::RUNNING) stopMotor("dry_run_guard");
        FLM_LOG_WARN("RelayMotor", "dry-run guard %.1f%%", pct);
    }
    _dryRunBlocked = blocked;
    return blocked;
}

void RelayMotorController::checkMaxRuntime() {
    if ((millis() - _startedAt) >= (uint32_t)_config.maxRunMinutes * 60000UL) {
        FLM_LOG_WARN("RelayMotor", "max runtime exceeded");
        stopMotor("max_runtime");
        _mode = MotorMode::OFF;
    }
}

String RelayMotorController::getStatusJson() const {
    JsonDocument doc;
    doc["controlType"]  = "relay";
    doc["enabled"]      = _config.enabled;
    doc["mode"]         = modeToString(_mode);
    doc["state"]        = stateToString(_state);
    doc["runSeconds"]   = getRunSeconds();
    doc["autoEnabled"]  = (_mode == MotorMode::AUTO);
    doc["dryRunBlocked"] = _dryRunBlocked;
    doc["levelPercent"] = _lastPct;
    doc["pin"]          = _config.pin;
    JsonObject t = doc["thresholds"].to<JsonObject>();
    t["onPercent"]     = _config.pumpOnPercent;
    t["offPercent"]    = _config.pumpOffPercent;
    t["maxRunMinutes"] = _config.maxRunMinutes;
    t["dryRunGuardPct"]= MOTOR_DRY_RUN_GUARD_PCT;
    String out; serializeJson(doc, out); return out;
}

String RelayMotorController::getMQTTJson() const {
    JsonDocument doc;
    doc["controlType"] = "relay";
    doc["state"]       = stateToString(_state);
    doc["mode"]        = modeToString(_mode);
    doc["runSeconds"]  = getRunSeconds();
    doc["blocked"]     = _dryRunBlocked;
    String out; serializeJson(doc, out); return out;
}
