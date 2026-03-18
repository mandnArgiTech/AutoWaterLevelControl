/**
 * @file SmsMotorController.h
 * @brief SMS-based motor controller via SIM800L/A6 GSM shield
 *
 * Used for Taro Smart Panel at Sangareddy farm (5HP motor).
 * Compiled when: -D FLM_ROLE_MOTOR_SMS
 *
 * The Taro Smart Panel accepts specific SMS messages to control the motor.
 * Both the ON and OFF message strings are fully configurable via config.json
 * and the app (MotorSmsConfigScreen), because different Taro panels may use
 * different command strings.
 *
 * Sequence:
 *   commandOn()  → send onMessage  SMS to targetPhone
 *   commandOff() → send offMessage SMS to targetPhone
 *   State = PENDING until GSM responds OK (or timeout → STOPPED + alert)
 *
 * @author FluidLevelMonitor Project
 * @version 2.0.0
 */
#ifndef SMS_MOTOR_CONTROLLER_H
#define SMS_MOTOR_CONTROLLER_H

#include "IMotorController.h"
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include "../utils/Log.h"

#define GSM_RESPONSE_TIMEOUT_MS  15000    ///< AT command response timeout
#define GSM_SEND_RETRIES         2        ///< Retry count on SMS send failure
#define GSM_INTER_RETRY_MS       5000     ///< Delay between retries

class SmsMotorController : public IMotorController {
public:
    SmsMotorController() = default;
    ~SmsMotorController() override;

    ErrorCode  begin(const MotorConfig& config) override;
    void       loop(float percentFilled) override;
    void       setMode(MotorMode mode) override;
    MotorMode  getMode()         const override { return _mode; }
    MotorState getState()        const override { return _state; }
    uint32_t   getRunSeconds()   const override;
    bool       isDryRunBlocked() const override { return _dryRunBlocked; }
    String     getStatusJson()   const override;
    String     getMQTTJson()     const override;
    String     getControlType()  const override { return "sms"; }

    /** Last SMS send result for status reporting */
    bool lastSmsSent() const { return _lastSmsSent; }
    String lastSmsError() const { return _lastSmsError; }

private:
    bool sendSms(const String& message);
    bool waitForResponse(const String& expected, uint32_t timeoutMs);
    void flushGsm();
    bool initGsm();
    void commandOn();
    void commandOff();
    bool checkDryRun(float pct);
    void checkMaxRuntime();

    MotorConfig    _config       = {};
    MotorMode      _mode         = MotorMode::OFF;
    MotorState     _state        = MotorState::STOPPED;
    bool           _initialized  = false;
    bool           _dryRunBlocked = false;
    bool           _lastSmsSent  = false;
    String         _lastSmsError;
    unsigned long  _startedAt    = 0;
    unsigned long  _pendingSince = 0;
    float          _lastPct      = 0;
    SoftwareSerial* _gsm         = nullptr;
};
#endif
