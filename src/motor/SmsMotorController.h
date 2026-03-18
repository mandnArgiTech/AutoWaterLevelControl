/**
 * @file SmsMotorController.h
 * @brief SMS-based motor controller via SIM800L/A6 GSM shield
 */
#ifndef SMS_MOTOR_CONTROLLER_H
#define SMS_MOTOR_CONTROLLER_H

#include "IMotorController.h"
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include "../utils/Log.h"

#define GSM_INIT_RETRY_MS       30000u
#define GSM_OFF_RETRY_MS        45000u   ///< Re-try OFF SMS while motor still physically on

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

    bool lastSmsSent() const { return _lastSmsSent; }
    String lastSmsError() const { return _lastSmsError; }

private:
    enum class SmsPhase : uint8_t {
        Idle,
        WaitPrompt,
        WaitCmgs,
        RetryWait
    };
    enum class SmsJob : uint8_t { None, On, Off };

    void flushGsm();
    bool initGsmBlocking();
    void tryGsmInit();
    void pollSmsFsm();
    void startSmsJob(SmsJob job, const String& body);
    void onSmsSuccess();
    void onSmsFailure();
    void commandOn();
    void commandOff();
    bool checkDryRun(float pct);
    void checkMaxRuntime();
    bool bufferHas(const String& buf, const char* needle) const;

    MotorConfig    _config        = {};
    MotorMode      _mode          = MotorMode::OFF;
    MotorState     _state         = MotorState::STOPPED;
    bool           _initialized   = false;
    bool           _gsmOk         = false;
    unsigned long  _nextGsmInitMs = 0;
    bool           _dryRunBlocked = false;
    bool           _lastSmsSent   = false;
    String         _lastSmsError;
    unsigned long  _startedAt     = 0;
    unsigned long  _pendingSince  = 0;
    float          _lastPct       = 0;
    SoftwareSerial* _gsm          = nullptr;

    SmsJob         _smsJob        = SmsJob::None;
    SmsPhase       _smsPhase      = SmsPhase::Idle;
    uint8_t        _smsAttempt    = 0;
    String         _smsBody;
    unsigned long  _smsDeadline   = 0;
    String         _rxLine;
    unsigned long  _nextOffRetryMs = 0;
    bool           _pendingOffRetry  = false;
};
#endif
