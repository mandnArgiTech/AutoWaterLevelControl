/**
 * @file WebServer.cpp
 * @brief Web server core: routes, static files, JSON helpers
 */
#include "WebServer.h"
#include "../utils/Log.h"

WebServerManager::WebServerManager(ISensor& sensor, TankCalculator& calculator,
                                   FlmOtaPrepareFn otaPrepare, FlmOtaRestoreFn otaRestore)
    : _server(ConfigManager::getInstance().getSystemConfig().webPort)
    , _sensor(sensor)
    , _calculator(calculator)
    , _running(false)
    , _requestCount(0)
    , _firmwareUploadOk(false)
    , _otaPrepare(otaPrepare)
    , _otaRestore(otaRestore) {
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

    _server.on("/api/wifi/status", HTTP_GET, std::bind(&WebServerManager::handleApiWiFiStatus, this));
    _server.on("/api/wifi/scan", HTTP_GET, std::bind(&WebServerManager::handleApiWiFiScan, this));

    _server.on("/api/mqtt/status", HTTP_GET, std::bind(&WebServerManager::handleApiMQTTStatus, this));

    _server.on("/api/time", HTTP_GET, std::bind(&WebServerManager::handleApiTimeStatus, this));

    _server.on("/api/errors", HTTP_GET, std::bind(&WebServerManager::handleApiErrors, this));
    _server.on("/api/errors/clear", HTTP_POST, std::bind(&WebServerManager::handleApiErrorsClear, this));

    _server.on("/api/restart", HTTP_POST, std::bind(&WebServerManager::handleApiRestart, this));
    _server.on("/api/reset", HTTP_POST, std::bind(&WebServerManager::handleApiReset, this));

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
    if (!handleFileRead("/index.html")) {
        String html = F("<!DOCTYPE html><html><head>");
        html += F("<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>");
        html += F("<title>FluidLevelMonitor</title></head><body>");
        html += F("<h1>FluidLevelMonitor</h1>");
        html += F("<p>Upload index.html to LittleFS for full web interface.</p>");
        html += F("<p><a href='/api/status'>API Status</a></p>");
        html += F("</body></html>");
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
    String contentType = getContentType(path);
    String pathWithGz = path + ".gz";
    if (LittleFS.exists(pathWithGz) || LittleFS.exists(path)) {
        if (LittleFS.exists(pathWithGz)) {
            path = pathWithGz;
        }
        File file = LittleFS.open(path, "r");
        if (file) {
            if (path.endsWith(".gz")) {
                _server.sendHeader("Content-Encoding", "gzip");
            }
            _server.streamFile(file, contentType);
            file.close();
            return true;
        }
    }
    return false;
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
