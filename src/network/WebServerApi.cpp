/**
 * @file WebServerApi.cpp
 * @brief REST API handlers for WebServerManager
 */
#include "WebServer.h"
#include "WiFiManager.h"
#include "MQTTManager.h"
#include "../utils/TimeManager.h"
#include "../version.h"

void WebServerManager::handleApiStatus() {
    _requestCount++;
    addCorsHeaders();

    JsonDocument doc;
    doc["device"] = ConfigManager::getInstance().getSystemConfig().deviceName;
    doc["firmware"] = Version::getFirmware();
    doc["uptime"] = TimeManager::getInstance().getUptimeString();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["heapFragmentation"] = ESP.getHeapFragmentation();
    doc["maxFreeBlock"] = ESP.getMaxFreeBlockSize();
    doc["timestamp"] = TimeManager::getInstance().getISO8601();

    doc["sensorOk"] = _calculator.getLastLevel().sensorOk;
    doc["sensorType"] = _sensor.getSensorTypeName();

    WaterLevel level = _calculator.getLastLevel();
    JsonObject levelObj = doc["level"].to<JsonObject>();
    levelObj["valid"] = level.valid;
    levelObj["sensorOk"] = level.sensorOk;
    if (level.sensorOk) {
        levelObj["percentFilled"] = level.percentFilled;
        levelObj["percentRemaining"] = level.percentRemaining;
        levelObj["waterHeightCm"] = level.waterHeightCm;
        levelObj["volumeLiters"] = level.volumeLiters;
        levelObj["volumeRemaining"] = level.volumeRemaining;
        levelObj["state"] = TankCalculator::tankStateToString(_calculator.getTankState());
    } else {
        levelObj["state"] = "sensor_error";
        levelObj["errorCode"] = static_cast<uint16_t>(level.error);
        levelObj["errorDescription"] = ErrorHandler::getInstance().getErrorDescription(level.error);
    }

    JsonObject connection = doc["connection"].to<JsonObject>();
    connection["wifi"] = WiFiManager::getInstance().isConnected();
    connection["mqtt"] = MQTTManager::getInstance().isConnected();
    connection["ip"] = WiFiManager::getInstance().getIP();

    String output;
    serializeJson(doc, output);
    sendJson(200, output);
}

void WebServerManager::handleApiLevel() {
    _requestCount++;
    addCorsHeaders();
    _calculator.calculate();
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
    doc["flashSize"] = ESP.getFlashChipSize();
    doc["sdkVersion"] = ESP.getSdkVersion();

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
        } else {
            sendApiFailure(400, "SECTION_INVALID", "Failed to parse or validate section");
        }
        return;
    }

    result = ConfigManager::getInstance().saveConfig();
    ConfigManager::getInstance().unlockConfigWrite();

    if (result == ErrorCode::ERR_NONE) {
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
// PUMP API  — Phase 2 (RelayManager)
// =============================================================================
#include "../relay/RelayManager.h"

void WebServerManager::handleApiPumpGet() {
    _requestCount++;
    addCorsHeaders();
    sendJson(200, RelayManager::getInstance().getStatusJson());
}

void WebServerManager::handleApiPumpPost() {
    _requestCount++;
    addCorsHeaders();

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

    RelayManager::getInstance().setMode(RelayManager::modeFromString(state));
    sendSuccess("Pump mode set to " + state);
}
