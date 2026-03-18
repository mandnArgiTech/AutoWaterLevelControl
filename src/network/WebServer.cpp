/**
 * @file WebServer.cpp
 * @brief Implementation of web server and REST API
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#include "WebServer.h"
#include "../network/WiFiManager.h"
#include "../network/MQTTManager.h"
#include "../utils/TimeManager.h"
#include "../version.h"

// =============================================================================
// SECTION 1: CONSTRUCTOR
// =============================================================================

/**
 * @brief Constructor with dependencies
 * @param sensor Reference to ISensor
 * @param calculator Reference to TankCalculator
 */
WebServerManager::WebServerManager(ISensor& sensor, TankCalculator& calculator)
    : _server(ConfigManager::getInstance().getSystemConfig().webPort)
    , _sensor(sensor)
    , _calculator(calculator)
    , _running(false)
    , _requestCount(0) {
}

// =============================================================================
// SECTION 2: INITIALIZATION
// =============================================================================

/**
 * @brief Initialize web server
 * @return ErrorCode indicating success or failure
 */
ErrorCode WebServerManager::begin() {
    Serial.println(F("[WebServer] Initializing..."));
    
    // Step 1: Setup routes
    setupRoutes();
    
    // Step 2: Start server
    _server.begin();
    _running = true;
    
    Serial.printf("[WebServer] Started on port %d\n", 
                  ConfigManager::getInstance().getSystemConfig().webPort);
    
    return ErrorCode::ERR_NONE;
}

/**
 * @brief Setup all route handlers
 */
void WebServerManager::setupRoutes() {
    // Step 1: Static files
    _server.on("/", HTTP_GET, std::bind(&WebServerManager::handleRoot, this));
    
    // Step 2: API - Status endpoints
    _server.on("/api/status", HTTP_GET, std::bind(&WebServerManager::handleApiStatus, this));
    _server.on("/api/level", HTTP_GET, std::bind(&WebServerManager::handleApiLevel, this));
    _server.on("/api/sensor", HTTP_GET, std::bind(&WebServerManager::handleApiSensor, this));
    _server.on("/api/info", HTTP_GET, std::bind(&WebServerManager::handleApiInfo, this));
    
    // Step 3: API - Configuration endpoints
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
    
    // Step 4: API - WiFi endpoints
    _server.on("/api/wifi/status", HTTP_GET, std::bind(&WebServerManager::handleApiWiFiStatus, this));
    _server.on("/api/wifi/scan", HTTP_GET, std::bind(&WebServerManager::handleApiWiFiScan, this));
    
    // Step 5: API - MQTT endpoint
    _server.on("/api/mqtt/status", HTTP_GET, std::bind(&WebServerManager::handleApiMQTTStatus, this));
    
    // Step 6: API - Time endpoint
    _server.on("/api/time", HTTP_GET, std::bind(&WebServerManager::handleApiTimeStatus, this));
    
    // Step 7: API - Error endpoints
    _server.on("/api/errors", HTTP_GET, std::bind(&WebServerManager::handleApiErrors, this));
    _server.on("/api/errors/clear", HTTP_POST, std::bind(&WebServerManager::handleApiErrorsClear, this));
    
    // Step 8: API - System endpoints
    _server.on("/api/restart", HTTP_POST, std::bind(&WebServerManager::handleApiRestart, this));
    _server.on("/api/reset", HTTP_POST, std::bind(&WebServerManager::handleApiReset, this));
    
    // Step 9: CORS preflight handler
    _server.on("/api/config", HTTP_OPTIONS, [this]() {
        addCorsHeaders();
        _server.send(204);
    });
    
    // Step 10: Not found handler
    _server.onNotFound(std::bind(&WebServerManager::handleNotFound, this));
}

// =============================================================================
// SECTION 3: LOOP PROCESSING
// =============================================================================

/**
 * @brief Process web server requests
 */
void WebServerManager::loop() {
    _server.handleClient();
}

// =============================================================================
// SECTION 4: STATIC FILE HANDLERS
// =============================================================================

/**
 * @brief Handle root request
 */
void WebServerManager::handleRoot() {
    _requestCount++;
    if (!handleFileRead("/index.html")) {
        // Serve embedded minimal page if no index.html
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

/**
 * @brief Handle file not found
 */
void WebServerManager::handleNotFound() {
    _requestCount++;
    
    // Try to serve file from LittleFS
    if (!handleFileRead(_server.uri())) {
        sendError(404, "Not Found");
    }
}

/**
 * @brief Read and serve file from LittleFS
 * @param path File path
 * @return true if file served
 */
bool WebServerManager::handleFileRead(String path) {
    // Append index.html for directory requests
    if (path.endsWith("/")) {
        path += "index.html";
    }
    
    String contentType = getContentType(path);
    
    // Check for gzipped version
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

/**
 * @brief Get content type for file
 * @param filename File name
 * @return MIME type string
 */
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

// =============================================================================
// SECTION 5: API - STATUS ENDPOINTS
// =============================================================================

/**
 * @brief Handle GET /api/status
 */
void WebServerManager::handleApiStatus() {
    _requestCount++;
    addCorsHeaders();
    
    JsonDocument doc;
    
    // Device info
    doc["device"] = ConfigManager::getInstance().getSystemConfig().deviceName;
    doc["firmware"] = Version::getFirmware();
    doc["uptime"] = TimeManager::getInstance().getUptimeString();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["timestamp"] = TimeManager::getInstance().getISO8601();
    
    // Sensor status
    doc["sensorOk"] = _calculator.getLastLevel().sensorOk;
    doc["sensorType"] = _sensor.getSensorTypeName();
    
    // Water level
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
    
    // Connection status
    JsonObject connection = doc["connection"].to<JsonObject>();
    connection["wifi"] = WiFiManager::getInstance().isConnected();
    connection["mqtt"] = MQTTManager::getInstance().isConnected();
    connection["ip"] = WiFiManager::getInstance().getIP();
    
    String output;
    serializeJson(doc, output);
    sendJson(200, output);
}

/**
 * @brief Handle GET /api/level
 */
void WebServerManager::handleApiLevel() {
    _requestCount++;
    addCorsHeaders();
    
    _calculator.calculate();
    sendJson(200, _calculator.getWaterLevelJson());
}

/**
 * @brief Handle GET /api/sensor
 */
void WebServerManager::handleApiSensor() {
    _requestCount++;
    addCorsHeaders();
    sendJson(200, _sensor.getStatusJson());
}

/**
 * @brief Handle GET /api/info
 */
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

// =============================================================================
// SECTION 6: API - CONFIGURATION ENDPOINTS
// =============================================================================

/**
 * @brief Handle GET /api/config
 */
void WebServerManager::handleApiConfigGet() {
    _requestCount++;
    addCorsHeaders();
    sendJson(200, ConfigManager::getInstance().getConfigJson());
}

/**
 * @brief Handle POST /api/config
 */
void WebServerManager::handleApiConfigPost() {
    _requestCount++;
    addCorsHeaders();
    
    if (!_server.hasArg("plain")) {
        sendError(400, "No body provided");
        return;
    }
    
    String body = _server.arg("plain");
    ErrorCode result = ConfigManager::getInstance().setConfigFromJson(body);
    
    if (result == ErrorCode::ERR_NONE) {
        result = ConfigManager::getInstance().saveConfig();
    }
    
    if (result == ErrorCode::ERR_NONE) {
        sendSuccess("Configuration saved");
    } else {
        sendError(400, "Failed to save configuration");
    }
}

/**
 * @brief Handle GET /api/config/{section}
 */
void WebServerManager::handleApiConfigSectionGet() {
    _requestCount++;
    addCorsHeaders();
    
    String uri = _server.uri();
    String section = uri.substring(uri.lastIndexOf('/') + 1);
    
    String json = ConfigManager::getInstance().getSectionJson(section);
    sendJson(200, json);
}

/**
 * @brief Handle POST /api/config/{section}
 */
void WebServerManager::handleApiConfigSectionPost() {
    _requestCount++;
    addCorsHeaders();
    
    if (!_server.hasArg("plain")) {
        sendError(400, "No body provided");
        return;
    }
    
    String uri = _server.uri();
    String section = uri.substring(uri.lastIndexOf('/') + 1);
    String body = _server.arg("plain");
    
    ErrorCode result = ConfigManager::getInstance().updateSection(section, body);
    
    if (result == ErrorCode::ERR_NONE) {
        result = ConfigManager::getInstance().saveConfig();
    }
    
    if (result == ErrorCode::ERR_NONE) {
        sendSuccess("Section " + section + " saved");
    } else {
        sendError(400, "Failed to save " + section);
    }
}

// =============================================================================
// SECTION 7: API - WIFI ENDPOINTS
// =============================================================================

/**
 * @brief Handle GET /api/wifi/status
 */
void WebServerManager::handleApiWiFiStatus() {
    _requestCount++;
    addCorsHeaders();
    sendJson(200, WiFiManager::getInstance().getStatusJson());
}

/**
 * @brief Handle GET /api/wifi/scan
 */
void WebServerManager::handleApiWiFiScan() {
    _requestCount++;
    addCorsHeaders();
    sendJson(200, WiFiManager::getInstance().scanNetworks());
}

// =============================================================================
// SECTION 8: API - MQTT ENDPOINT
// =============================================================================

/**
 * @brief Handle GET /api/mqtt/status
 */
void WebServerManager::handleApiMQTTStatus() {
    _requestCount++;
    addCorsHeaders();
    sendJson(200, MQTTManager::getInstance().getStatusJson());
}

// =============================================================================
// SECTION 9: API - TIME ENDPOINT
// =============================================================================

/**
 * @brief Handle GET /api/time
 */
void WebServerManager::handleApiTimeStatus() {
    _requestCount++;
    addCorsHeaders();
    sendJson(200, TimeManager::getInstance().getStatusJson());
}

// =============================================================================
// SECTION 10: API - ERROR ENDPOINTS
// =============================================================================

/**
 * @brief Handle GET /api/errors
 */
void WebServerManager::handleApiErrors() {
    _requestCount++;
    addCorsHeaders();
    sendJson(200, ErrorHandler::getInstance().getErrorHistoryJson());
}

/**
 * @brief Handle POST /api/errors/clear
 */
void WebServerManager::handleApiErrorsClear() {
    _requestCount++;
    addCorsHeaders();
    
    ErrorHandler::getInstance().clearErrors();
    sendSuccess("Errors cleared");
}

// =============================================================================
// SECTION 11: API - SYSTEM ENDPOINTS
// =============================================================================

/**
 * @brief Handle POST /api/restart
 */
void WebServerManager::handleApiRestart() {
    _requestCount++;
    addCorsHeaders();
    
    sendSuccess("Restarting device...");
    delay(500);
    ESP.restart();
}

/**
 * @brief Handle POST /api/reset
 */
void WebServerManager::handleApiReset() {
    _requestCount++;
    addCorsHeaders();
    
    // Reset configuration to defaults
    ConfigManager::getInstance().resetToDefaults();
    ConfigManager::getInstance().saveConfig();
    
    sendSuccess("Factory reset complete. Restarting...");
    delay(500);
    ESP.restart();
}

// =============================================================================
// SECTION 12: HELPER METHODS
// =============================================================================

/**
 * @brief Send JSON response
 * @param code HTTP status code
 * @param json JSON string
 */
void WebServerManager::sendJson(int code, const String& json) {
    _server.send(code, "application/json", json);
}

/**
 * @brief Send error response
 * @param code HTTP status code
 * @param message Error message
 */
void WebServerManager::sendError(int code, const String& message) {
    JsonDocument doc;
    doc["error"] = true;
    doc["message"] = message;
    doc["code"] = code;
    
    String output;
    serializeJson(doc, output);
    sendJson(code, output);
}

/**
 * @brief Send success response
 * @param message Success message
 */
void WebServerManager::sendSuccess(const String& message) {
    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = message;
    
    String output;
    serializeJson(doc, output);
    sendJson(200, output);
}

/**
 * @brief Add CORS headers to response
 */
void WebServerManager::addCorsHeaders() {
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    _server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    _server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

