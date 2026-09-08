/**
 * @file BootDiagnostics.h
 * @brief Capture ESP8266 reset cause, persist last reboot, heartbeat previous session.
 *
 * Answers: power fail vs crash vs watchdog vs intentional restart.
 * Exposed on GET /api/status under "reboot".
 */
#ifndef FLM_BOOT_DIAGNOSTICS_H
#define FLM_BOOT_DIAGNOSTICS_H

#include <Arduino.h>
#include <ArduinoJson.h>

class BootDiagnostics {
public:
    static BootDiagnostics& getInstance() {
        static BootDiagnostics inst;
        return inst;
    }

    /** Call once after LittleFS is mounted (ConfigManager::begin). */
    void begin();

    /** Periodic: persist uptime/heap so the *next* boot can report previous session. */
    void loop();

    /** Mark next restart as intentional (API/MQTT/OTA/factory reset). */
    void markIntentionalRestart(const char* note);

    void fillStatus(JsonObject obj) const;

    const char* category() const { return _category; }
    const char* reason() const { return _reason.c_str(); }
    const char* likelyCause() const { return _likelyCause; }
    bool intentional() const { return _intentional; }
    uint32_t reasonCode() const { return _reasonCode; }
    uint32_t bootCount() const { return _bootCount; }
    uint32_t previousUptimeMs() const { return _prevUptimeMs; }

private:
    BootDiagnostics() = default;

    void readHardwareReset();
    void loadIntentFile();
    void loadHeartbeat();
    void loadBootCount();
    void saveBootCount();
    void classify();
    void persistLastRecord();
    void saveHeartbeat();

    String _reason;
    String _info;
    String _intentNote;
    const char* _category = "UNKNOWN";
    const char* _likelyCause = "Unknown reset cause";
    uint32_t _reasonCode = 0;
    uint32_t _exccause = 0;
    uint32_t _epc1 = 0;
    uint32_t _epc2 = 0;
    uint32_t _epc3 = 0;
    uint32_t _excvaddr = 0;
    uint32_t _depc = 0;
    bool _intentional = false;
    bool _hasExceptionRegs = false;

    uint32_t _bootCount = 0;
    uint32_t _prevUptimeMs = 0;
    uint32_t _prevFreeHeap = 0;
    uint32_t _prevMinFreeHeap = 0;
    float _prevBatteryV = -1.f;
    bool _prevHeartbeatValid = false;

    unsigned long _lastHeartbeatMs = 0;
};

#endif
