/**
 * @file BootDiagnostics.cpp
 * @brief ESP8266 reboot / crash / power-fail diagnostics
 */
#include "BootDiagnostics.h"
#include "HeapMonitor.h"
#include "Log.h"
#include "TimeManager.h"
#include <LittleFS.h>
#include <user_interface.h>

#ifdef FLM_BATTERY_MONITOR
#include "BatteryMonitor.h"
#endif

static const char* HEARTBEAT_PATH = "/reboot_heartbeat.json";
static const char* INTENT_PATH = "/reboot_intent.json";
static const char* BOOTCOUNT_PATH = "/boot_count.txt";
static const char* LAST_PATH = "/reboot_last.json";
static const unsigned long HEARTBEAT_INTERVAL_MS = 60000UL;

void BootDiagnostics::begin() {
    readHardwareReset();
    loadIntentFile();
    loadHeartbeat();
    loadBootCount();
    _bootCount++;
    saveBootCount();
    classify();
    persistLastRecord();

    FLM_LOG_WARN("Boot", "reset #%u code=%u reason='%s' category=%s intentional=%d",
                 (unsigned)_bootCount, (unsigned)_reasonCode, _reason.c_str(),
                 _category, _intentional ? 1 : 0);
    FLM_LOG_WARN("Boot", "likely: %s", _likelyCause);
    if (_info.length()) {
        FLM_LOG_WARN("Boot", "info: %s", _info.c_str());
    }
    if (_prevHeartbeatValid) {
        FLM_LOG_INFO("Boot", "previous uptimeMs=%u freeHeap=%u minFreeHeap=%u",
                     (unsigned)_prevUptimeMs, (unsigned)_prevFreeHeap,
                     (unsigned)_prevMinFreeHeap);
    }

    // Drop intent after consuming so a later crash is not mislabeled.
    LittleFS.remove(INTENT_PATH);
    saveHeartbeat();  // seed heartbeat immediately
}

void BootDiagnostics::loop() {
    unsigned long now = millis();
    if ((now - _lastHeartbeatMs) >= HEARTBEAT_INTERVAL_MS || _lastHeartbeatMs == 0) {
        saveHeartbeat();
        _lastHeartbeatMs = now;
    }
}

void BootDiagnostics::markIntentionalRestart(const char* note) {
    JsonDocument doc;
    doc["note"] = note ? note : "intentional";
    doc["uptimeMs"] = millis();
    doc["at"] = TimeManager::getInstance().getUptimeString();
    File f = LittleFS.open(INTENT_PATH, "w");
    if (!f) {
        FLM_LOG_WARN("Boot", "could not write intent file");
        return;
    }
    serializeJson(doc, f);
    f.flush();
    f.close();
    saveHeartbeat();
    FLM_LOG_INFO("Boot", "marked intentional restart: %s", note ? note : "");
}

void BootDiagnostics::readHardwareReset() {
    _reason = ESP.getResetReason();
    _info = ESP.getResetInfo();
    const rst_info* ri = ESP.getResetInfoPtr();
    if (ri) {
        _reasonCode = ri->reason;
        if (ri->reason == REASON_EXCEPTION_RST) {
            _hasExceptionRegs = true;
            _exccause = ri->exccause;
            _epc1 = ri->epc1;
            _epc2 = ri->epc2;
            _epc3 = ri->epc3;
            _excvaddr = ri->excvaddr;
            _depc = ri->depc;
        }
    }
}

void BootDiagnostics::loadIntentFile() {
    if (!LittleFS.exists(INTENT_PATH)) return;
    File f = LittleFS.open(INTENT_PATH, "r");
    if (!f) return;
    JsonDocument doc;
    if (deserializeJson(doc, f) == DeserializationError::Ok) {
        _intentional = true;
        if (doc["note"].is<const char*>()) {
            _intentNote = doc["note"].as<const char*>();
        }
    }
    f.close();
}

void BootDiagnostics::loadHeartbeat() {
    if (!LittleFS.exists(HEARTBEAT_PATH)) return;
    File f = LittleFS.open(HEARTBEAT_PATH, "r");
    if (!f) return;
    JsonDocument doc;
    if (deserializeJson(doc, f) == DeserializationError::Ok) {
        _prevHeartbeatValid = true;
        _prevUptimeMs = doc["uptimeMs"] | 0U;
        _prevFreeHeap = doc["freeHeap"] | 0U;
        _prevMinFreeHeap = doc["minFreeHeap"] | 0U;
        if (doc["batteryV"].is<float>()) {
            _prevBatteryV = doc["batteryV"].as<float>();
        }
    }
    f.close();
}

void BootDiagnostics::loadBootCount() {
    if (!LittleFS.exists(BOOTCOUNT_PATH)) return;
    File f = LittleFS.open(BOOTCOUNT_PATH, "r");
    if (!f) return;
    String s = f.readString();
    f.close();
    _bootCount = (uint32_t)s.toInt();
}

void BootDiagnostics::saveBootCount() {
    File f = LittleFS.open(BOOTCOUNT_PATH, "w");
    if (!f) return;
    f.print(_bootCount);
    f.close();
}

void BootDiagnostics::saveHeartbeat() {
    HeapMonitor::sample();
    JsonDocument doc;
    doc["uptimeMs"] = millis();
    doc["freeHeap"] = HeapMonitor::freeHeap();
    doc["minFreeHeap"] = HeapMonitor::minFreeHeap();
#ifdef FLM_BATTERY_MONITOR
    doc["batteryV"] = BatteryMonitor::getInstance().readVoltageCached(5000);
#endif
    File f = LittleFS.open(HEARTBEAT_PATH, "w");
    if (!f) return;
    serializeJson(doc, f);
    f.close();
}

void BootDiagnostics::classify() {
    // Intentional software restart wins over raw REASON_SOFT_RESTART.
    if (_intentional) {
        _category = "INTENTIONAL";
        if (_intentNote.indexOf("ota") >= 0 || _intentNote.indexOf("OTA") >= 0 ||
            _intentNote.indexOf("firmware") >= 0) {
            _likelyCause = "Intentional reboot after firmware OTA/update";
        } else if (_intentNote.indexOf("factory") >= 0) {
            _likelyCause = "Intentional factory reset restart";
        } else if (_intentNote.indexOf("mqtt") >= 0) {
            _likelyCause = "Intentional restart via MQTT command";
        } else if (_intentNote.indexOf("api") >= 0 || _intentNote.indexOf("web") >= 0) {
            _likelyCause = "Intentional restart via web/API";
        } else {
            _likelyCause = "Intentional software restart (marked before reboot)";
        }
        return;
    }

    switch (_reasonCode) {
        case REASON_DEFAULT_RST:  // 0 power-on
            _category = "POWER";
            _likelyCause = "Power-on reset — supply was removed/restored or brownout recovered as cold boot";
            break;
        case REASON_WDT_RST:  // 1
            _category = "WATCHDOG";
            _likelyCause = "Hardware watchdog reset — firmware hung (possible crash/deadlock)";
            break;
        case REASON_EXCEPTION_RST:  // 2
            _category = "CRASH";
            _likelyCause = "CPU exception/panic — firmware crash (see exceptionCause/epc1)";
            break;
        case REASON_SOFT_WDT_RST:  // 3
            _category = "WATCHDOG";
            _likelyCause = "Software watchdog reset — loop blocked too long without yield";
            break;
        case REASON_SOFT_RESTART:  // 4
            _category = "SOFTWARE";
            _likelyCause = "Software restart (ESP.restart) without intent marker — OTA tool, SDK, or unmarked code path";
            break;
        case REASON_DEEP_SLEEP_AWAKE:  // 5
            _category = "SLEEP";
            _likelyCause = "Wake from deep sleep";
            break;
        case REASON_EXT_SYS_RST:  // 6
            _category = "EXTERNAL";
            _likelyCause = "External reset pin / button (or adapter glitch seen as EXT_RST)";
            break;
        default:
            _category = "UNKNOWN";
            _likelyCause = "Unrecognized reset reason — check info string";
            break;
    }

    // Low previous heap strongly suggests OOM-related failure before this boot.
    if (_prevHeartbeatValid && _prevMinFreeHeap > 0 && _prevMinFreeHeap < 8192 &&
        (strcmp(_category, "CRASH") == 0 || strcmp(_category, "WATCHDOG") == 0 ||
         strcmp(_category, "SOFTWARE") == 0)) {
        _likelyCause = "Likely out-of-memory related (previous minFreeHeap was very low before reset)";
    }

#ifdef FLM_BATTERY_MONITOR
    if (_prevHeartbeatValid && _prevBatteryV > 0.f && _prevBatteryV < 3.2f &&
        (strcmp(_category, "POWER") == 0 || _reasonCode == REASON_DEFAULT_RST)) {
        _likelyCause = "Likely battery/brownout (previous battery voltage was low before reset)";
    }
#endif
}

void BootDiagnostics::persistLastRecord() {
    JsonDocument doc;
    fillStatus(doc.to<JsonObject>());
    File f = LittleFS.open(LAST_PATH, "w");
    if (!f) return;
    serializeJson(doc, f);
    f.close();
}

void BootDiagnostics::fillStatus(JsonObject obj) const {
    obj["reasonCode"] = _reasonCode;
    obj["reason"] = _reason;
    obj["category"] = _category;
    obj["likelyCause"] = _likelyCause;
    obj["info"] = _info;
    obj["intentional"] = _intentional;
    if (_intentNote.length()) obj["intentNote"] = _intentNote;
    obj["bootCount"] = _bootCount;
    if (_hasExceptionRegs) {
        obj["exceptionCause"] = _exccause;
        obj["epc1"] = _epc1;
        obj["epc2"] = _epc2;
        obj["epc3"] = _epc3;
        obj["excvaddr"] = _excvaddr;
        obj["depc"] = _depc;
    }
    if (_prevHeartbeatValid) {
        obj["previousUptimeMs"] = _prevUptimeMs;
        // human string
        uint32_t s = _prevUptimeMs / 1000UL;
        uint32_t d = s / 86400UL;
        uint32_t h = (s % 86400UL) / 3600UL;
        uint32_t m = (s % 3600UL) / 60UL;
        uint32_t sec = s % 60UL;
        char buf[48];
        if (d > 0) {
            snprintf(buf, sizeof(buf), "%ud %uh %um %us", d, h, m, sec);
        } else if (h > 0) {
            snprintf(buf, sizeof(buf), "%uh %um %us", h, m, sec);
        } else {
            snprintf(buf, sizeof(buf), "%um %us", m, sec);
        }
        obj["previousUptime"] = buf;
        obj["previousFreeHeap"] = _prevFreeHeap;
        obj["previousMinFreeHeap"] = _prevMinFreeHeap;
        if (_prevBatteryV > 0.f) obj["previousBatteryV"] = _prevBatteryV;
    }
}
