/**
 * @file RelayManager.h
 * @brief Pump relay control with AUTO mode, safety cutoffs, and dry-run guard
 *
 * Features:
 * - GPIO relay control (configurable pin, active-low or active-high)
 * - Three operating modes: OFF | AUTO | ON (manual)
 * - AUTO mode: turns pump ON when level < pumpOnPercent,
 *              turns pump OFF when level >= pumpOffPercent
 * - Dry-run guard: never runs pump when percentFilled < DRY_RUN_GUARD_PCT
 * - Max-runtime safety: auto-stops after maxRunMinutes
 * - REST API: GET/POST /api/pump
 * - MQTT: publishes to {deviceTag}/water/pump
 *         receives commands via existing /command handler
 *
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#ifndef RELAY_MANAGER_H
#define RELAY_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../config/ConfigManager.h"
#include "../utils/ErrorHandler.h"
#include "../utils/Log.h"

// Safety constants — never configurable from API
#define DRY_RUN_GUARD_PCT       5.0f    ///< Never run if filled < 5%
#define RELAY_MAX_RUN_MINUTES   60      ///< Hard cap (config may lower this)
#define RELAY_STATUS_INTERVAL   30000   ///< MQTT pump status publish interval ms

enum class PumpMode : uint8_t {
    OFF  = 0,   ///< Always off (manual)
    AUTO = 1,   ///< Automatic based on thresholds
    ON   = 2,   ///< Always on (manual override)
};

enum class PumpState : uint8_t {
    STOPPED  = 0,
    RUNNING  = 1,
    DISABLED = 2,   ///< Dry-run guard or safety cutoff active
};

struct RelayConfig {
    bool    enabled;            ///< Feature enabled flag
    int     pin;                ///< GPIO pin for relay
    bool    activeLow;          ///< true = LOW turns relay ON
    float   pumpOnPercent;      ///< AUTO: start pump when level below this
    float   pumpOffPercent;     ///< AUTO: stop pump when level at or above this
    uint8_t maxRunMinutes;      ///< Safety cutoff (capped at RELAY_MAX_RUN_MINUTES)
};

class RelayManager {
public:
    static RelayManager& getInstance();

    /**
     * @brief Initialize relay GPIO and load config
     * @param config Relay configuration
     */
    ErrorCode begin(const RelayConfig& config);

    /**
     * @brief Call from main loop — evaluates AUTO mode, enforces safety cutoffs
     * @param percentFilled Current tank fill level (0–100)
     */
    void loop(float percentFilled);

    // ── Control ──────────────────────────────────────────────────────────────

    /** Set operating mode. In ON mode, pump starts if dry-run guard allows. */
    void setMode(PumpMode mode);

    /** Current mode */
    PumpMode getMode() const { return _mode; }

    /** Physical pump state */
    PumpState getPumpState() const { return _pumpState; }

    /** Seconds the pump has been running in the current session */
    uint32_t getRunSeconds() const;

    /** true if the dry-run guard is currently blocking the pump */
    bool isDryRunBlocked() const { return _dryRunBlocked; }

    // ── JSON ─────────────────────────────────────────────────────────────────

    /** GET /api/pump response body */
    String getStatusJson() const;

    /** MQTT pump payload */
    String getMQTTJson() const;

    // ── Helpers ──────────────────────────────────────────────────────────────

    static String modeToString(PumpMode mode);
    static String stateToString(PumpState state);
    static PumpMode modeFromString(const String& s);

private:
    RelayManager() = default;
    RelayManager(const RelayManager&) = delete;
    RelayManager& operator=(const RelayManager&) = delete;

    void startPump();
    void stopPump(const char* reason);
    void setRelay(bool on);
    bool checkDryRun(float percentFilled);
    void checkMaxRuntime();

    RelayConfig  _config     = {};
    PumpMode     _mode       = PumpMode::OFF;
    PumpState    _pumpState  = PumpState::STOPPED;
    bool         _initialized    = false;
    bool         _dryRunBlocked  = false;
    unsigned long _startedAt     = 0;   ///< millis() when pump last started
    unsigned long _lastStatusMs  = 0;
    float        _lastPercent    = 0;
};

#endif // RELAY_MANAGER_H
