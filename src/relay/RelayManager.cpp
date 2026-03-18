/**
 * @file RelayManager.cpp
 * @brief Pump relay control implementation
 *
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#include "RelayManager.h"

// =============================================================================
// SECTION 1: SINGLETON
// =============================================================================

RelayManager& RelayManager::getInstance() {
    static RelayManager instance;
    return instance;
}

// =============================================================================
// SECTION 2: INIT
// =============================================================================

ErrorCode RelayManager::begin(const RelayConfig& config) {
    _config = config;

    if (!config.enabled) {
        FLM_LOG_INFO("Relay", "disabled");
        _pumpState = PumpState::DISABLED;
        return ErrorCode::ERR_NONE;
    }

    // Clamp maxRunMinutes to hard safety cap
    if (_config.maxRunMinutes > RELAY_MAX_RUN_MINUTES) {
        FLM_LOG_WARN("Relay", "maxRunMinutes clamped %d->%d",
                     _config.maxRunMinutes, RELAY_MAX_RUN_MINUTES);
        _config.maxRunMinutes = RELAY_MAX_RUN_MINUTES;
    }

    // Configure GPIO — start with relay OFF
    pinMode(_config.pin, OUTPUT);
    setRelay(false);

    _mode       = PumpMode::AUTO;
    _pumpState  = PumpState::STOPPED;
    _initialized = true;

    FLM_LOG_INFO("Relay", "ready pin=%d activeLow=%d onPct=%.0f offPct=%.0f maxRun=%dmin",
                 _config.pin, _config.activeLow,
                 _config.pumpOnPercent, _config.pumpOffPercent,
                 _config.maxRunMinutes);

    return ErrorCode::ERR_NONE;
}

// =============================================================================
// SECTION 3: LOOP
// =============================================================================

void RelayManager::loop(float percentFilled) {
    if (!_initialized || _pumpState == PumpState::DISABLED) return;

    _lastPercent = percentFilled;

    // Dry-run guard — overrides everything
    if (checkDryRun(percentFilled)) return;

    // Max-runtime safety cutoff
    if (_pumpState == PumpState::RUNNING) {
        checkMaxRuntime();
        if (_pumpState != PumpState::RUNNING) return;
    }

    // AUTO mode logic
    if (_mode == PumpMode::AUTO) {
        if (_pumpState == PumpState::STOPPED && percentFilled < _config.pumpOnPercent) {
            FLM_LOG_INFO("Relay", "AUTO: level %.1f%% < on-threshold %.1f%% — starting pump",
                         percentFilled, _config.pumpOnPercent);
            startPump();
        } else if (_pumpState == PumpState::RUNNING && percentFilled >= _config.pumpOffPercent) {
            FLM_LOG_INFO("Relay", "AUTO: level %.1f%% >= off-threshold %.1f%% — stopping pump",
                         percentFilled, _config.pumpOffPercent);
            stopPump("auto_threshold_reached");
        }
    }

    // MANUAL ON — ensure pump is running if not blocked
    if (_mode == PumpMode::ON && _pumpState == PumpState::STOPPED) {
        startPump();
    }

    // MANUAL OFF — ensure pump is stopped
    if (_mode == PumpMode::OFF && _pumpState == PumpState::RUNNING) {
        stopPump("manual_off");
    }
}

// =============================================================================
// SECTION 4: CONTROL
// =============================================================================

void RelayManager::setMode(PumpMode mode) {
    if (_mode == mode) return;
    FLM_LOG_INFO("Relay", "mode: %s -> %s",
                 modeToString(_mode).c_str(), modeToString(mode).c_str());
    _mode = mode;

    // Immediate effect when switching to OFF
    if (mode == PumpMode::OFF && _pumpState == PumpState::RUNNING) {
        stopPump("mode_set_off");
    }
}

uint32_t RelayManager::getRunSeconds() const {
    if (_pumpState != PumpState::RUNNING || _startedAt == 0) return 0;
    return (millis() - _startedAt) / 1000UL;
}

// =============================================================================
// SECTION 5: INTERNAL HELPERS
// =============================================================================

void RelayManager::startPump() {
    if (_pumpState == PumpState::RUNNING) return;
    setRelay(true);
    _pumpState  = PumpState::RUNNING;
    _startedAt  = millis();
    FLM_LOG_INFO("Relay", "pump ON");
}

void RelayManager::stopPump(const char* reason) {
    if (_pumpState == PumpState::STOPPED) return;
    setRelay(false);
    _pumpState = PumpState::STOPPED;
    FLM_LOG_INFO("Relay", "pump OFF [%s] ran %us", reason, getRunSeconds());
    _startedAt = 0;
}

void RelayManager::setRelay(bool on) {
    // activeLow: relay ON = write LOW; activeLow false: relay ON = write HIGH
    digitalWrite(_config.pin, (_config.activeLow ? !on : on) ? HIGH : LOW);
}

bool RelayManager::checkDryRun(float percentFilled) {
    bool blocked = percentFilled < DRY_RUN_GUARD_PCT;
    if (blocked && !_dryRunBlocked) {
        FLM_LOG_WARN("Relay", "dry-run guard: level %.1f%% < %.1f%% — pump blocked",
                     percentFilled, DRY_RUN_GUARD_PCT);
        if (_pumpState == PumpState::RUNNING) {
            stopPump("dry_run_guard");
        }
    }
    _dryRunBlocked = blocked;
    return blocked;
}

void RelayManager::checkMaxRuntime() {
    uint32_t maxMs = (uint32_t)_config.maxRunMinutes * 60000UL;
    if ((millis() - _startedAt) >= maxMs) {
        FLM_LOG_WARN("Relay", "max runtime %d min exceeded — stopping pump",
                     _config.maxRunMinutes);
        stopPump("max_runtime");
        // Force OFF mode so loop doesn't immediately restart it
        _mode = PumpMode::OFF;
        ErrorHandler::getInstance().logError(ErrorCode::ERR_NONE,
            "Pump stopped: max runtime exceeded");
    }
}

// =============================================================================
// SECTION 6: JSON
// =============================================================================

String RelayManager::getStatusJson() const {
    JsonDocument doc;
    doc["enabled"]      = _config.enabled;
    doc["mode"]         = modeToString(_mode);
    doc["state"]        = stateToString(_pumpState);
    doc["runSeconds"]   = getRunSeconds();
    doc["autoEnabled"]  = (_mode == PumpMode::AUTO);
    doc["dryRunBlocked"] = _dryRunBlocked;
    doc["levelPercent"] = _lastPercent;
    doc["pin"]          = _config.pin;

    JsonObject thresholds = doc["thresholds"].to<JsonObject>();
    thresholds["onPercent"]      = _config.pumpOnPercent;
    thresholds["offPercent"]     = _config.pumpOffPercent;
    thresholds["maxRunMinutes"]  = _config.maxRunMinutes;
    thresholds["dryRunGuardPct"] = DRY_RUN_GUARD_PCT;

    String out; serializeJson(doc, out); return out;
}

String RelayManager::getMQTTJson() const {
    JsonDocument doc;
    doc["state"]      = stateToString(_pumpState);
    doc["mode"]       = modeToString(_mode);
    doc["runSeconds"] = getRunSeconds();
    doc["blocked"]    = _dryRunBlocked;
    String out; serializeJson(doc, out); return out;
}

// =============================================================================
// SECTION 7: STRING HELPERS
// =============================================================================

String RelayManager::modeToString(PumpMode mode) {
    switch (mode) {
        case PumpMode::OFF:  return "off";
        case PumpMode::AUTO: return "auto";
        case PumpMode::ON:   return "on";
        default:             return "unknown";
    }
}

String RelayManager::stateToString(PumpState state) {
    switch (state) {
        case PumpState::STOPPED:  return "stopped";
        case PumpState::RUNNING:  return "running";
        case PumpState::DISABLED: return "disabled";
        default:                  return "unknown";
    }
}

PumpMode RelayManager::modeFromString(const String& s) {
    if (s == "auto") return PumpMode::AUTO;
    if (s == "on")   return PumpMode::ON;
    return PumpMode::OFF;
}
