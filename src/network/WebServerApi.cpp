/**
 * @file WebServerApi.cpp
 * @brief REST API handlers for WebServerManager
 */
#include "WebServer.h"
#include "../motor/IMotorController.h"
#include "WiFiManager.h"
#include "MQTTManager.h"
#include "../utils/TimeManager.h"
#include "../utils/HeapMonitor.h"
#include "../version.h"
#include <LittleFS.h>
#ifdef FLM_BATTERY_MONITOR
#include "../utils/BatteryMonitor.h"
#endif

void WebServerManager::handleApiStatus() {
    _requestCount++;
    addCorsHeaders();

    // Compact dashboard payload — use cached sensor level only (main loop updates it).
    JsonDocument doc;
    doc["device"] = ConfigManager::getInstance().getMQTTConfig().deviceName;
    doc["firmware"] = Version::getFirmware();
    doc["uptime"] = TimeManager::getInstance().getUptimeString();
    HeapMonitor::sample();
    doc["freeHeap"] = HeapMonitor::freeHeap();
    doc["minFreeHeap"] = HeapMonitor::minFreeHeap();
    doc["maxFreeBlock"] = HeapMonitor::maxFreeBlock();

    const WaterLevel& level = _calculator.getLastLevel();
    doc["sensorOk"] = level.sensorOk;

    JsonObject levelObj = doc["level"].to<JsonObject>();
    levelObj["valid"] = level.valid;
    levelObj["sensorOk"] = level.sensorOk;
    levelObj["percentFilled"] = roundf(level.percentFilled * 10.0f) / 10.0f;
    levelObj["percentRemaining"] = roundf(level.percentRemaining * 10.0f) / 10.0f;
    levelObj["waterHeightMm"] = roundf(level.waterHeightMm);
    levelObj["waterHeightCm"] = roundf(level.waterHeightCm * 10.0f) / 10.0f;
    levelObj["volumeLiters"] = roundf(level.volumeLiters * 10.0f) / 10.0f;
    levelObj["volumeRemaining"] = roundf(level.volumeRemaining * 10.0f) / 10.0f;
    levelObj["distanceMm"] = roundf(level.distanceMm);
    levelObj["distanceCm"] = roundf(level.distanceCm * 10.0f) / 10.0f;
    levelObj["tankHeightMm"] = ConfigManager::getInstance().getTankConfig().height;
    levelObj["offsetMm"] = ConfigManager::getInstance().getSensorConfig().offsetMm;
    levelObj["state"] = level.sensorOk && level.valid
        ? TankCalculator::tankStateToString(_calculator.getTankState())
        : "sensor_error";
    if (level.temperatureValid) {
        levelObj["temperatureC"] = roundf(level.temperatureC * 10.0f) / 10.0f;
    }
    if (level.humidityValid) {
        levelObj["humidityPct"] = roundf(level.humidityPct * 10.0f) / 10.0f;
    }

    JsonObject connection = doc["connection"].to<JsonObject>();
    connection["wifi"] = WiFiManager::getInstance().isConnected();
    connection["mqtt"] = MQTTManager::getInstance().isConnected();
    connection["mac"] = WiFiManager::getInstance().getMAC();
    doc["mac"] = WiFiManager::getInstance().getMAC();
    {
        JsonObject mqtt = doc["mqtt"].to<JsonObject>();
        MQTTManager::getInstance().fillStatus(mqtt);
    }

#ifdef FLM_BATTERY_MONITOR
    {
        // Cached battery snapshot — avoid 8× ADC + delay(2) on every poll.
        const float v = BatteryMonitor::getInstance().readVoltageCached(3000);
        JsonObject batt = doc["battery"].to<JsonObject>();
        batt["voltage"] = roundf(v * 100.0f) / 100.0f;
        batt["percent"] = BatteryMonitor::getInstance().getStateOfCharge(v);
    }
#endif

    String output;
    serializeJson(doc, output);
    sendJson(200, output);
}

void WebServerManager::handleApiLevel() {
    _requestCount++;
    addCorsHeaders();
    // Never recalculate here — that re-runs filters and stalls WiFi under poll load.
    sendJson(200, _calculator.getWaterLevelJson());
}

void WebServerManager::handleApiSensor() {
    _requestCount++;
    addCorsHeaders();
    sendJson(200, _sensor.getStatusJson());
}

void WebServerManager::handleApiInfo() {
    _requestCount++;
    addCorsHeaders();

    JsonDocument doc;
    doc["project"] = PROJECT_NAME;
    doc["description"] = PROJECT_DESCRIPTION;
    doc["version"] = Version::getVersion();
    doc["build"] = Version::getBuild();
    doc["firmware"] = Version::getFirmware();
    doc["buildDate"] = Version::getBuildDate();
    doc["buildTime"] = Version::getBuildTime();
    doc["device"] = Version::getDeviceType();
    doc["chipId"] = String(ESP.getChipId(), HEX);
    doc["mac"] = WiFiManager::getInstance().getMAC();
    doc["flashSize"] = ESP.getFlashChipSize();
    doc["sdkVersion"] = ESP.getSdkVersion();
    {
        size_t totalBytes = 0, usedBytes = 0;
        ConfigManager::getInstance().getFilesystemInfo(totalBytes, usedBytes);
        doc["fsTotal"] = totalBytes;
        doc["fsUsed"] = usedBytes;
        doc["fsFree"] = (totalBytes > usedBytes) ? (totalBytes - usedBytes) : 0;
    }

    String output;
    serializeJson(doc, output);
    sendJson(200, output);
}

void WebServerManager::handleApiConfigGet() {
    _requestCount++;
    addCorsHeaders();
    sendJson(200, ConfigManager::getInstance().getConfigJson());
}

void WebServerManager::handleApiConfigPost() {
    _requestCount++;
    addCorsHeaders();

    if (!_server.hasArg("plain")) {
        sendApiFailure(400, "NO_BODY", "No body provided");
        return;
    }
    if (!ConfigManager::getInstance().tryLockForConfigWrite()) {
        sendApiFailure(503, "CONFIG_LOCKED", "Configuration write in progress; retry shortly");
        return;
    }

    String body = _server.arg("plain");
    ErrorCode result = ConfigManager::getInstance().setConfigFromJson(body);
    if (result != ErrorCode::ERR_NONE) {
        ConfigManager::getInstance().unlockConfigWrite();
        if (result == ErrorCode::ERR_WEB_REQUEST) {
            sendApiFailure(413, "PAYLOAD_TOO_LARGE", "JSON body exceeds limit");
        } else if (result == ErrorCode::ERR_CONFIG_PARSE) {
            sendApiFailure(400, "CONFIG_PARSE", "Invalid JSON");
        } else {
            sendApiFailure(400, "CONFIG_VALIDATE", "Configuration validation failed");
        }
        return;
    }

    result = ConfigManager::getInstance().saveConfig();
    ConfigManager::getInstance().unlockConfigWrite();

    if (result == ErrorCode::ERR_NONE) {
        sendSuccess("Configuration saved");
    } else if (result == ErrorCode::ERR_FS_FULL) {
        sendApiFailure(507, "FS_FULL", "Filesystem full; free space and retry");
    } else {
        sendApiFailure(500, "CONFIG_SAVE", "Failed to save configuration");
    }
}

void WebServerManager::handleApiConfigSectionGet() {
    _requestCount++;
    addCorsHeaders();

    String uri = _server.uri();
    String section = uri.substring(uri.lastIndexOf('/') + 1);

    String json = ConfigManager::getInstance().getSectionJson(section);
    sendJson(200, json);
}

void WebServerManager::handleApiConfigSectionPost() {
    _requestCount++;
    addCorsHeaders();

    if (!_server.hasArg("plain")) {
        sendApiFailure(400, "NO_BODY", "No body provided");
        return;
    }
    if (!ConfigManager::getInstance().tryLockForConfigWrite()) {
        sendApiFailure(503, "CONFIG_LOCKED", "Configuration write in progress; retry shortly");
        return;
    }

    String uri = _server.uri();
    String section = uri.substring(uri.lastIndexOf('/') + 1);
    String body = _server.arg("plain");

    ErrorCode result = ConfigManager::getInstance().updateSection(section, body);
    if (result != ErrorCode::ERR_NONE) {
        ConfigManager::getInstance().unlockConfigWrite();
        if (result == ErrorCode::ERR_WEB_REQUEST) {
            sendApiFailure(413, "PAYLOAD_TOO_LARGE", "JSON body exceeds limit");
        } else if (result == ErrorCode::ERR_CONFIG_PARSE) {
            sendApiFailure(400, "SECTION_PARSE", "Invalid JSON for section");
        } else if (result == ErrorCode::ERR_CONFIG_VALIDATE) {
            sendApiFailure(400, "SECTION_INVALID", "Section values failed validation");
        } else {
            sendApiFailure(400, "SECTION_INVALID", "Failed to apply section");
        }
        return;
    }

    result = ConfigManager::getInstance().saveConfig();
    ConfigManager::getInstance().unlockConfigWrite();

    if (result == ErrorCode::ERR_NONE) {
        if (section == "sensor") {
            // Apply offset immediately so calibration takes effect without reboot.
            _sensor.setCalibrationOffset(
                ConfigManager::getInstance().getSensorConfig().offsetMm);
        }
        sendSuccess("Section " + section + " saved");
    } else if (result == ErrorCode::ERR_FS_FULL) {
        sendApiFailure(507, "FS_FULL", "Filesystem full");
    } else {
        sendApiFailure(500, "CONFIG_SAVE", "Failed to save");
    }
}

void WebServerManager::handleApiWiFiStatus() {
    _requestCount++;
    addCorsHeaders();
    sendJson(200, WiFiManager::getInstance().getStatusJson());
}

void WebServerManager::handleApiWiFiScan() {
    _requestCount++;
    addCorsHeaders();
    sendJson(200, WiFiManager::getInstance().scanNetworks());
}

void WebServerManager::handleApiMQTTStatus() {
    _requestCount++;
    addCorsHeaders();
    sendJson(200, MQTTManager::getInstance().getStatusJson());
}

void WebServerManager::handleApiMQTTLog() {
    _requestCount++;
    addCorsHeaders();
    uint16_t limit = MQTT_LOG_CAPACITY;
    if (_server.hasArg("limit")) {
        int v = _server.arg("limit").toInt();
        if (v > 0 && v <= MQTT_LOG_CAPACITY) {
            limit = (uint16_t)v;
        }
    }
    sendJson(200, MQTTManager::getInstance().getLogJson(limit));
}

/**
 * POST /api/mqtt/ca — install broker CA certificate (PEM in request body).
 * DELETE /api/mqtt/ca — remove it.
 * Restart required for the change to take effect.
 */
void WebServerManager::handleApiMQTTCaPost() {
    _requestCount++;
    addCorsHeaders();

    if (!_server.hasArg("plain")) {
        sendApiFailure(400, "NO_BODY", "PEM certificate body required");
        return;
    }
    String pem = _server.arg("plain");
    if (pem.indexOf("-----BEGIN CERTIFICATE-----") < 0) {
        sendApiFailure(400, "BAD_PEM", "Body is not a PEM certificate");
        return;
    }

    File f = LittleFS.open(MQTT_CA_FILE, "w");
    if (!f) {
        sendApiFailure(500, "FS_WRITE", "Cannot open CA file for writing");
        return;
    }
    size_t written = f.print(pem);
    f.close();
    if (written != pem.length()) {
        LittleFS.remove(MQTT_CA_FILE);
        sendApiFailure(507, "FS_FULL", "Filesystem full while writing CA");
        return;
    }
    sendSuccess("CA certificate saved; restart to apply");
}

void WebServerManager::handleApiMQTTCaDelete() {
    _requestCount++;
    addCorsHeaders();
    LittleFS.remove(MQTT_CA_FILE);
    sendSuccess("CA certificate removed; restart to apply");
}

void WebServerManager::handleApiTimeStatus() {
    _requestCount++;
    addCorsHeaders();
    sendJson(200, TimeManager::getInstance().getStatusJson());
}

void WebServerManager::handleApiErrors() {
    _requestCount++;
    addCorsHeaders();
    sendJson(200, ErrorHandler::getInstance().getErrorHistoryJson());
}

void WebServerManager::handleApiErrorsClear() {
    _requestCount++;
    addCorsHeaders();
    ErrorHandler::getInstance().clearErrors();
    sendSuccess("Errors cleared");
}

void WebServerManager::handleApiRestart() {
    _requestCount++;
    addCorsHeaders();
    sendSuccess("Restarting device...");
    delay(500);
    ESP.restart();
}

void WebServerManager::handleApiReset() {
    _requestCount++;
    addCorsHeaders();

    if (!ConfigManager::getInstance().tryLockForConfigWrite()) {
        sendApiFailure(503, "CONFIG_LOCKED", "Retry factory reset shortly");
        return;
    }
    ConfigManager::getInstance().resetToDefaults();
    (void)ConfigManager::getInstance().saveConfig();
    ConfigManager::getInstance().unlockConfigWrite();

    sendSuccess("Factory reset complete. Restarting...");
    delay(500);
    ESP.restart();
}

// =============================================================================
// PUMP API — IMotorController (motor_relay / motor_sms builds only)
// =============================================================================

void WebServerManager::handleApiPumpGet() {
    _requestCount++;
    addCorsHeaders();
    if (!_motor) {
        sendApiFailure(503, "NO_MOTOR", "Pump control not available on this firmware role");
        return;
    }
    sendJson(200, _motor->getStatusJson());
}

void WebServerManager::handleApiPumpPost() {
    _requestCount++;
    addCorsHeaders();
    if (!_motor) {
        sendApiFailure(503, "NO_MOTOR", "Pump control not available on this firmware role");
        return;
    }

    if (!_server.hasArg("plain")) {
        sendApiFailure(400, "NO_BODY", "No body provided");
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, _server.arg("plain"))) {
        sendApiFailure(400, "PARSE_ERROR", "Invalid JSON");
        return;
    }

    String state = doc["state"] | "";
    if (state.isEmpty()) {
        sendApiFailure(400, "MISSING_STATE", "Required field: state (on|off|auto)");
        return;
    }

    _motor->setMode(IMotorController::modeFromString(state));
    sendSuccess("Pump mode set to " + state);
}

#include "DistanceCalibratePage.h"
#include <math.h>

void WebServerManager::handleDistanceCalibratePage() {
    _requestCount++;
    _server.send_P(200, "text/html", WEB_PAGE_DISTANCE_CALIBRATE);
}

void WebServerManager::handleApiDistanceCalibrateGet() {
    _requestCount++;
    addCorsHeaders();

    // Ensure UART sensors have fresh frames before sampling raw.
    _sensor.poll();
    const float rawMm = _sensor.readRawDistanceMm();
    const float offsetMm = _sensor.getCalibrationOffset();

    JsonDocument doc;
    const bool ok = (rawMm > 0.f);
    doc["ok"] = ok;
    doc["rawMm"] = ok ? roundf(rawMm) : (float)-1;
    doc["offsetMm"] = roundf(offsetMm * 10.f) / 10.f;
    doc["calibratedMm"] = ok ? roundf(rawMm + offsetMm) : (float)-1;
    doc["tankHeightMm"] = ConfigManager::getInstance().getTankConfig().height;
    doc["filtered"] = false;
    doc["sensor"] = _sensor.getSensorTypeName();

    String output;
    serializeJson(doc, output);
    sendJson(200, output);
}

void WebServerManager::handleApiDistanceCalibratePost() {
    _requestCount++;
    addCorsHeaders();

    if (!_server.hasArg("plain")) {
        sendApiFailure(400, "NO_BODY", "No body provided");
        return;
    }

    JsonDocument body;
    if (deserializeJson(body, _server.arg("plain"))) {
        sendApiFailure(400, "BAD_JSON", "Invalid JSON");
        return;
    }

    _sensor.poll();
    const float rawMm = _sensor.readRawDistanceMm();
    float newOffset = 0.f;
    float measuredMm = NAN;

    if (!body["offsetMm"].isNull()) {
        // Explicit offset (e.g. reset to 0)
        newOffset = body["offsetMm"].as<float>();
    } else if (!body["measuredMm"].isNull()) {
        measuredMm = body["measuredMm"].as<float>();
        if (!(measuredMm > 0.f) || measuredMm > 30000.f) {
            sendApiFailure(400, "BAD_MEASURE", "measuredMm must be between 1 and 30000");
            return;
        }
        if (!(rawMm > 0.f)) {
            sendApiFailure(503, "NO_RAW", "No raw sensor reading — wait for a stable distance");
            return;
        }
        // Tape = raw + offset  →  offset = tape − raw
        newOffset = measuredMm - rawMm;
    } else {
        sendApiFailure(400, "MISSING", "Provide measuredMm (tape) or offsetMm");
        return;
    }

    if (newOffset < -2000.f || newOffset > 2000.f) {
        sendApiFailure(400, "OFFSET_RANGE", "Computed offset out of range (±2000 mm) — check tape/raw units");
        return;
    }

    if (!ConfigManager::getInstance().tryLockForConfigWrite()) {
        sendApiFailure(503, "CONFIG_LOCKED", "Configuration write in progress; retry shortly");
        return;
    }

    JsonDocument section;
    section["offsetMm"] = newOffset;
    String sectionJson;
    serializeJson(section, sectionJson);

    ErrorCode result = ConfigManager::getInstance().updateSection("sensor", sectionJson);
    if (result != ErrorCode::ERR_NONE) {
        ConfigManager::getInstance().unlockConfigWrite();
        sendApiFailure(400, "SECTION_INVALID", "Failed to apply offset");
        return;
    }

    result = ConfigManager::getInstance().saveConfig();
    ConfigManager::getInstance().unlockConfigWrite();

    if (result != ErrorCode::ERR_NONE) {
        if (result == ErrorCode::ERR_FS_FULL) {
            sendApiFailure(507, "FS_FULL", "Filesystem full");
        } else {
            sendApiFailure(500, "CONFIG_SAVE", "Failed to save");
        }
        return;
    }

    _sensor.setCalibrationOffset(newOffset);

    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = "Distance calibration saved";
    doc["rawMm"] = (rawMm > 0.f) ? roundf(rawMm) : (float)-1;
    doc["measuredMm"] = !isnan(measuredMm) ? roundf(measuredMm) : (float)-1;
    doc["offsetMm"] = roundf(newOffset * 10.f) / 10.f;
    doc["calibratedMm"] = (rawMm > 0.f) ? roundf(rawMm + newOffset) : (float)-1;
    String output;
    serializeJson(doc, output);
    sendJson(200, output);
}

#ifdef FLM_BATTERY_MONITOR
#include "BatteryCalibratePage.h"
#include "../utils/BatteryMonitor.h"
#include "../utils/adc_scaling.h"

void WebServerManager::handleBatteryCalibratePage() {
    _requestCount++;
    _server.send_P(200, "text/html", WEB_PAGE_BATTERY_CALIBRATE);
}

void WebServerManager::handleApiBatteryCalibrateGet() {
    _requestCount++;
    addCorsHeaders();
    auto& batt = BatteryMonitor::getInstance();
    int raw = batt.readAnalogRawAveraged(16);
    float measured = batt.readBatteryVoltage(raw);
    JsonDocument doc;
    doc["ok"] = true;
    doc["source"] = "a0";
    doc["source_label"] = "ESP8266 A0";
    doc["raw"] = raw;
    doc["measured"] = measured;
    doc["offset"] = batt.getCalibrationOffset();
    doc["a0PinV"] = adcRawToA0PinVoltage(raw);
    doc["cells"] = ConfigManager::getInstance().getBatteryConfig().cellsInSeries;
    doc["scale"] = adcBatteryVoltageRatio();
    doc["maxMeasurableV"] = adcMaxBatteryVoltage();
    String out;
    serializeJson(doc, out);
    sendJson(200, out);
}

void WebServerManager::handleApiBatteryCalibratePost() {
    _requestCount++;
    addCorsHeaders();
    if (!_server.hasArg("plain")) {
        sendApiFailure(400, "NO_BODY", "No body provided");
        return;
    }
    JsonDocument req;
    if (deserializeJson(req, _server.arg("plain"))) {
        sendApiFailure(400, "PARSE_ERROR", "Invalid JSON");
        return;
    }

    auto& batt = BatteryMonitor::getInstance();
    bool reset = req["reset"] | false;

    if (reset) {
        batt.setCalibrationOffset(0.0f);
    } else {
        float actual = req["actual"] | 0.0f;
        if (!(actual > 0.0f)) {
            sendApiFailure(400, "INVALID_ACTUAL", "invalid actual voltage");
            return;
        }
        // Match proven flow: clear offset, average 16 samples, then calibrate
        batt.setCalibrationOffset(0.0f);
        int raw = batt.readAnalogRawAveraged(16);
        float measured = batt.readBatteryVoltage(raw);
        batt.calibrate(actual, measured);
    }

    BatteryConfig& cfg = ConfigManager::getInstance().getBatteryConfig();
    cfg.calibrationOffset = batt.getCalibrationOffset();
    cfg.autoCalibration = !reset;
    ErrorCode saveRc = ConfigManager::getInstance().saveConfig();
    if (saveRc != ErrorCode::ERR_NONE) {
        sendApiFailure(503, "CONFIG_LOCKED", "Could not save calibration");
        return;
    }

    JsonDocument resp;
    resp["ok"] = true;
    resp["offset"] = batt.getCalibrationOffset();
    if (!reset) {
        resp["actual"] = req["actual"];
        // Re-read with new offset for response measured field (pre-cal measured)
        batt.setCalibrationOffset(0.0f);
        float measured = batt.readBatteryVoltage(batt.readAnalogRawAveraged(16));
        batt.setCalibrationOffset(cfg.calibrationOffset);
        resp["measured"] = measured;
    }
    String out;
    serializeJson(resp, out);
    sendJson(200, out);
}
#endif

