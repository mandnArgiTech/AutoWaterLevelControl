/**
 * @file RelayMotorController.h
 * @brief GPIO relay motor controller (home, small motors)
 *
 * Compiled when: -D FLM_ROLE_MOTOR_RELAY
 * Refactored from RelayManager; now implements IMotorController.
 *
 * @author FluidLevelMonitor Project
 * @version 2.0.0
 */
#ifndef RELAY_MOTOR_CONTROLLER_H
#define RELAY_MOTOR_CONTROLLER_H

#include "IMotorController.h"
#include "../utils/Log.h"
#include <ArduinoJson.h>

class RelayMotorController : public IMotorController {
public:
    RelayMotorController() = default;
    ~RelayMotorController() override = default;

    ErrorCode  begin(const MotorConfig& config) override;
    void       loop(float percentFilled) override;
    void       setMode(MotorMode mode) override;
    MotorMode  getMode()         const override { return _mode; }
    MotorState getState()        const override { return _state; }
    uint32_t   getRunSeconds()   const override;
    bool       isDryRunBlocked() const override { return _dryRunBlocked; }
    String     getStatusJson()   const override;
    String     getMQTTJson()     const override;
    String     getControlType()  const override { return "relay"; }

private:
    void startMotor();
    void stopMotor(const char* reason);
    void setRelay(bool on);
    bool checkDryRun(float pct);
    void checkMaxRuntime();

    MotorConfig   _config       = {};
    MotorMode     _mode         = MotorMode::OFF;
    MotorState    _state        = MotorState::STOPPED;
    bool          _initialized  = false;
    bool          _dryRunBlocked = false;
    unsigned long _startedAt    = 0;
    float         _lastPct      = 0;
};
#endif
