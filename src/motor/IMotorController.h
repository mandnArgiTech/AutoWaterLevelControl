/**
 * @file IMotorController.h
 * @brief Abstract motor controller interface
 *
 * Two concrete implementations:
 *   RelayMotorController  — GPIO relay (home, small motors)
 *   SmsMotorController    — GSM/SIM800L SMS to Taro Smart Panel (farm 5HP)
 *
 * The firmware role is selected at compile time:
 *   -D FLM_ROLE_MOTOR_RELAY   → RelayMotorController
 *   -D FLM_ROLE_MOTOR_SMS     → SmsMotorController
 *
 * @author FluidLevelMonitor Project
 * @version 2.0.0
 */

#ifndef IMOTOR_CONTROLLER_H
#define IMOTOR_CONTROLLER_H

#include <Arduino.h>
#include "../utils/ErrorHandler.h"

enum class MotorMode : uint8_t {
    OFF  = 0,   ///< Always off (manual)
    AUTO = 1,   ///< Cloud-commanded or threshold-based
    ON   = 2,   ///< Manual on (override)
};

enum class MotorState : uint8_t {
    STOPPED  = 0,
    RUNNING  = 1,
    DISABLED = 2,   ///< Dry-run guard or safety cutoff
    PENDING  = 3,   ///< SMS sent, awaiting confirmation
};

struct MotorConfig {
    bool    enabled;
    float   pumpOnPercent;      ///< AUTO: start when level below this
    float   pumpOffPercent;     ///< AUTO: stop when level at or above this
    uint8_t maxRunMinutes;      ///< Hard safety cutoff

    // Relay-specific
    int     pin;
    bool    activeLow;

    // SMS-specific
    int     gsmRxPin;
    int     gsmTxPin;
    int     gsmBaudRate;
    String  targetPhone;
    String  onMessage;          ///< SMS text to turn pump ON  (e.g. "START PUMP")
    String  offMessage;         ///< SMS text to turn pump OFF (e.g. "STOP PUMP")
    uint32_t confirmTimeoutMs;  ///< Max wait for SMS send confirmation
};

// Hard constants
constexpr float    MOTOR_DRY_RUN_GUARD_PCT = 5.0f;
constexpr uint8_t  MOTOR_MAX_RUN_MINUTES   = 60;

class IMotorController {
public:
    virtual ~IMotorController() = default;

    virtual ErrorCode begin(const MotorConfig& config) = 0;

    /**
     * Call from main loop. Evaluates AUTO thresholds and safety cutoffs.
     * @param percentFilled Current tank level (0–100). Pass -1 if unknown.
     */
    virtual void loop(float percentFilled) = 0;

    virtual void setMode(MotorMode mode) = 0;
    virtual MotorMode  getMode()       const = 0;
    virtual MotorState getState()      const = 0;
    virtual uint32_t   getRunSeconds() const = 0;
    virtual bool       isDryRunBlocked() const = 0;

    /** REST /api/pump response body */
    virtual String getStatusJson()  const = 0;
    /** MQTT {deviceTag}/motor/status payload */
    virtual String getMQTTJson()    const = 0;
    /** Human-readable control type string */
    virtual String getControlType() const = 0;

    static String modeToString(MotorMode m);
    static String stateToString(MotorState s);
    static MotorMode modeFromString(const String& s);
};

#endif // IMOTOR_CONTROLLER_H
