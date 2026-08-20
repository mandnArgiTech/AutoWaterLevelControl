/**
 * @file WebServer.cpp
 * @brief Web server core: routes, static files, JSON helpers
 */
#include "WebServer.h"
#include "../utils/Log.h"

WebServerManager::WebServerManager(ISensor& sensor, TankCalculator& calculator,
                                   FlmOtaPrepareFn otaPrepare, FlmOtaRestoreFn otaRestore,
                                   IMotorController* motor)
    : _server(ConfigManager::getInstance().getSystemConfig().webPort)
    , _sensor(sensor)
    , _calculator(calculator)
    , _running(false)
    , _requestCount(0)
    , _firmwareUploadOk(false)
    , _otaPrepare(otaPrepare)
    , _otaRestore(otaRestore)
    , _motor(motor) {
}

ErrorCode WebServerManager::begin() {
    FLM_LOG_INFO("Web", "Initializing...");
    setupRoutes();
    _server.begin();
    _running = true;
    FLM_LOG_INFO("Web", "port %u", (unsigned)ConfigManager::getInstance().getSystemConfig().webPort);
    return ErrorCode::ERR_NONE;
}

void WebServerManager::setupRoutes() {
    _server.on("/", HTTP_GET, std::bind(&WebServerManager::handleRoot, this));

    _server.on("/api/status", HTTP_GET, std::bind(&WebServerManager::handleApiStatus, this));
    _server.on("/api/level", HTTP_GET, std::bind(&WebServerManager::handleApiLevel, this));
    _server.on("/api/sensor", HTTP_GET, std::bind(&WebServerManager::handleApiSensor, this));
    _server.on("/api/info", HTTP_GET, std::bind(&WebServerManager::handleApiInfo, this));

    _server.on("/calibrate-distance", HTTP_GET, std::bind(&WebServerManager::handleDistanceCalibratePage, this));
    _server.on("/api/calibrate/distance", HTTP_GET, std::bind(&WebServerManager::handleApiDistanceCalibrateGet, this));
    _server.on("/api/calibrate/distance", HTTP_POST, std::bind(&WebServerManager::handleApiDistanceCalibratePost, this));

#ifdef FLM_BATTERY_MONITOR
    _server.on("/calibrate-battery", HTTP_GET, std::bind(&WebServerManager::handleBatteryCalibratePage, this));
    _server.on("/api/battery/calibrate", HTTP_GET, std::bind(&WebServerManager::handleApiBatteryCalibrateGet, this));
    _server.on("/api/battery/calibrate", HTTP_POST, std::bind(&WebServerManager::handleApiBatteryCalibratePost, this));
    _server.on("/api/config/battery", HTTP_GET, [this]() { handleApiConfigSectionGet(); });
    _server.on("/api/config/battery", HTTP_POST, [this]() { handleApiConfigSectionPost(); });
#endif

    _server.on("/api/config", HTTP_GET, std::bind(&WebServerManager::handleApiConfigGet, this));
    _server.on("/api/config", HTTP_POST, std::bind(&WebServerManager::handleApiConfigPost, this));
    _server.on("/api/config/wifi", HTTP_GET, [this]() { handleApiConfigSectionGet(); });
    _server.on("/api/config/wifi", HTTP_POST, [this]() { handleApiConfigSectionPost(); });
    _server.on("/api/config/mqtt", HTTP_GET, [this]() { handleApiConfigSectionGet(); });
    _server.on("/api/config/mqtt", HTTP_POST, [this]() { handleApiConfigSectionPost(); });
    _server.on("/api/config/tank", HTTP_GET, [this]() { handleApiConfigSectionGet(); });
    _server.on("/api/config/tank", HTTP_POST, [this]() { handleApiConfigSectionPost(); });
    _server.on("/api/config/sensor", HTTP_GET, [this]() { handleApiConfigSectionGet(); });
    _server.on("/api/config/sensor", HTTP_POST, [this]() { handleApiConfigSectionPost(); });
    _server.on("/api/config/system", HTTP_GET, [this]() { handleApiConfigSectionGet(); });
    _server.on("/api/config/system", HTTP_POST, [this]() { handleApiConfigSectionPost(); });
    _server.on("/api/config/relay", HTTP_GET, [this]() { handleApiConfigSectionGet(); });
    _server.on("/api/config/relay", HTTP_POST, [this]() { handleApiConfigSectionPost(); });

    _server.on("/api/wifi/status", HTTP_GET, std::bind(&WebServerManager::handleApiWiFiStatus, this));
    _server.on("/api/wifi/scan", HTTP_GET, std::bind(&WebServerManager::handleApiWiFiScan, this));

    _server.on("/api/mqtt/status", HTTP_GET, std::bind(&WebServerManager::handleApiMQTTStatus, this));
    _server.on("/api/mqtt/log", HTTP_GET, std::bind(&WebServerManager::handleApiMQTTLog, this));
    _server.on("/api/mqtt/ca", HTTP_POST, std::bind(&WebServerManager::handleApiMQTTCaPost, this));
    _server.on("/api/mqtt/ca", HTTP_DELETE, std::bind(&WebServerManager::handleApiMQTTCaDelete, this));

    _server.on("/api/time", HTTP_GET, std::bind(&WebServerManager::handleApiTimeStatus, this));

    _server.on("/api/errors", HTTP_GET, std::bind(&WebServerManager::handleApiErrors, this));
    _server.on("/api/errors/clear", HTTP_POST, std::bind(&WebServerManager::handleApiErrorsClear, this));

    _server.on("/api/restart", HTTP_POST, std::bind(&WebServerManager::handleApiRestart, this));
    _server.on("/api/reset",   HTTP_POST, std::bind(&WebServerManager::handleApiReset, this));

#if defined(FLM_ROLE_MOTOR_RELAY) || defined(FLM_ROLE_MOTOR_SMS)
    if (_motor) {
        _server.on("/api/pump", HTTP_GET,  std::bind(&WebServerManager::handleApiPumpGet,  this));
        _server.on("/api/pump", HTTP_POST, std::bind(&WebServerManager::handleApiPumpPost, this));
        _server.on("/api/pump", HTTP_OPTIONS, [this]() { addCorsHeaders(); _server.send(204); });
    }
#endif

    _server.on(
        "/api/update",
        HTTP_POST,
        [this]() { handleFirmwareUploadComplete(); },
        [this]() { handleFirmwareUpload(); });

    _server.on("/api/config", HTTP_OPTIONS, [this]() {
        addCorsHeaders();
        _server.send(204);
    });

    _server.onNotFound(std::bind(&WebServerManager::handleNotFound, this));
}

void WebServerManager::loop() {
    _server.handleClient();
}

void WebServerManager::stopForOTA() {
    if (!_running) return;
    _server.close();
    _running = false;
    FLM_LOG_INFO("Web", "stopped for OTA");
}

void WebServerManager::resumeAfterOTA() {
    if (_running) return;
    _server.begin();
    _running = true;
    FLM_LOG_INFO("Web", "resumed after OTA");
}

void WebServerManager::handleRoot() {
    _requestCount++;
    // Prefer gzip only — skip probing uncompressed first when .gz exists (already in handleFileRead).
    if (!handleFileRead("/index.html")) {
        String html = F("<!DOCTYPE html><html><head>");
        html += F("<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>");
        html += F("<title>FluidLevelMonitor</title></head><body>");
        html += F("<h1>FluidLevelMonitor</h1>");
        html += F("<p>Upload index.html to LittleFS for full web interface.</p>");
        html += F("<p><a href='/api/status'>API Status</a></p>");
#ifdef FLM_BATTERY_MONITOR
        html += F("<p><a href='/calibrate-battery'>Battery A0 Calibration</a></p>");
#endif
        html += F("</body></html>");
        _server.sendHeader("Connection", "close");
        _server.send(200, "text/html", html);
    }
}

void WebServerManager::handleNotFound() {
    _requestCount++;
    if (!handleFileRead(_server.uri())) {
        sendError(404, "Not Found");
    }
}

bool WebServerManager::handleFileRead(String path) {
    if (path.endsWith("/")) {
        path += "index.html";
    }
    // Content-Type from logical name (index.html), not the .gz path.
    String contentType = getContentType(path);
    String pathWithGz = path + ".gz";
    const bool gzipped = LittleFS.exists(pathWithGz);
    if (gzipped) {
        path = pathWithGz;
    } else if (!LittleFS.exists(path)) {
        return false;
    }

    File file = LittleFS.open(path, "r");
    if (!file) {
        return false;
    }

    const size_t fileSize = file.size();

    // Do NOT use streamFile() for *.gz — on ESP8266 it auto-adds Content-Encoding,
    // and any prior sendHeader("Content-Encoding") duplicates it. Browsers then fail
    // to decode (blank page / endless load). Send headers + body ourselves.
    _server.sendHeader(F("Connection"), F("close"));
    _server.sendHeader(F("Cache-Control"), gzipped ? F("public, max-age=60") : F("no-store"));
    if (gzipped) {
        _server.sendHeader(F("Content-Encoding"), F("gzip"));
    }
    _server.setContentLength(fileSize);
    // Use application/octet-stream for the streamFile suppress path is unnecessary —
    // we send an empty body header then write the file.
    _server.send(200, contentType, "");

    WiFiClient client = _server.client();
    uint8_t buf[512];
    while (file.available() && client.connected()) {
        size_t n = file.read(buf, sizeof(buf));
        if (n == 0) break;
        size_t off = 0;
        while (off < n && client.connected()) {
            size_t w = client.write(buf + off, n - off);
            if (w == 0) {
                yield();
                ESP.wdtFeed();
                continue;
            }
            off += w;
        }
        yield();
        ESP.wdtFeed();
    }
    file.close();
    client.stop();
    return true;
}

String WebServerManager::getContentType(const String& filename) {
    if (filename.endsWith(".html")) return "text/html";
    if (filename.endsWith(".css")) return "text/css";
    if (filename.endsWith(".js")) return "application/javascript";
    if (filename.endsWith(".json")) return "application/json";
    if (filename.endsWith(".png")) return "image/png";
    if (filename.endsWith(".gif")) return "image/gif";
    if (filename.endsWith(".jpg")) return "image/jpeg";
    if (filename.endsWith(".ico")) return "image/x-icon";
    if (filename.endsWith(".svg")) return "image/svg+xml";
    return "text/plain";
}

void WebServerManager::sendJson(int code, const String& json) {
    _server.sendHeader("Cache-Control", "no-store");
    _server.sendHeader("Connection", "close");
    _server.send(code, "application/json", json);
}

void WebServerManager::sendError(int code, const String& message) {
    JsonDocument doc;
    doc["error"] = true;
    doc["message"] = message;
    doc["code"] = code;
    String output;
    serializeJson(doc, output);
    sendJson(code, output);
}

void WebServerManager::sendApiFailure(int httpCode, const char* codeStr, const String& message) {
    JsonDocument doc;
    doc["success"] = false;
    doc["code"] = codeStr;
    doc["message"] = message;
    String output;
    serializeJson(doc, output);
    sendJson(httpCode, output);
}

void WebServerManager::sendSuccess(const String& message) {
    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = message;
    String output;
    serializeJson(doc, output);
    sendJson(200, output);
}

void WebServerManager::addCorsHeaders() {
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    _server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    _server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}
