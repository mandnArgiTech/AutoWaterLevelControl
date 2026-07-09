/**
 * @file WebServer.h
 * @brief Web server with REST API for configuration and monitoring
 * 
 * This module provides:
 * - Static file serving from LittleFS
 * - REST API for configuration
 * - Real-time status endpoints
 * - CORS support
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include "../config/ConfigManager.h"
#include "../utils/ErrorHandler.h"
#include "../sensor/ISensor.h"
#include "../tank/TankCalculator.h"

class IMotorController;

/** OTA hooks: prepare frees MQTT/WS (and optionally HTTP for Arduino OTA path). */
typedef void (*FlmOtaPrepareFn)();
typedef void (*FlmOtaRestoreFn)();

// =============================================================================
// SECTION 1: WEB SERVER CLASS
// =============================================================================

/**
 * @class WebServerManager
 * @brief Manages web server and REST API endpoints
 * 
 * Endpoints:
 * - GET /                  - Web interface
 * - GET /api/status        - Device status
 * - GET /api/level         - Current water level
 * - GET /api/config        - Get configuration
 * - POST /api/config       - Update configuration
 * - GET /api/config/{section} - Get config section
 * - POST /api/config/{section} - Update config section
 * - GET /api/wifi/scan     - Scan WiFi networks
 * - GET /api/errors        - Error history
 * - POST /api/errors/clear - Clear errors
 * - POST /api/restart      - Restart device
 * - POST /api/reset        - Factory reset
 * - POST /api/update       - Web firmware OTA (see WebServerFirmware.cpp)
 *
 * @note Implementation split: WebServer.cpp (core), WebServerApi.cpp, WebServerFirmware.cpp.
 */
class WebServerManager {
public:
    /**
     * @brief Constructor with dependencies
     * @param sensor Reference to ISensor (sensor-agnostic)
     * @param calculator Reference to TankCalculator
     * @param otaPrepare Called before firmware upload (stop MQTT/WS)
     * @param otaRestore Called on upload abort/failure
     */
    WebServerManager(ISensor& sensor, TankCalculator& calculator,
                     FlmOtaPrepareFn otaPrepare, FlmOtaRestoreFn otaRestore,
                     IMotorController* motor = nullptr);
    
    /**
     * @brief Initialize web server
     * @return ErrorCode indicating success or failure
     */
    ErrorCode begin();
    
    /**
     * @brief Process web server requests (call in loop)
     */
    void loop();
    
    /**
     * @brief Check if server is running
     * @return true if running
     */
    bool isRunning() const { return _running; }

    /** Stop HTTP server to free RAM before ArduinoOTA. */
    void stopForOTA();

    /** Restart HTTP server after failed ArduinoOTA. */
    void resumeAfterOTA();
    
    /**
     * @brief Get request count
     * @return Number of requests handled
     */
    uint32_t getRequestCount() const { return _requestCount; }

private:
    // Setup route handlers
    void setupRoutes();
    
    // Static file handlers
    void handleRoot();
    void handleNotFound();
    bool handleFileRead(String path);
    String getContentType(const String& filename);
    
    // API handlers - Status
    void handleApiStatus();
    void handleApiLevel();
    void handleApiSensor();
    
    // API handlers - Configuration
    void handleApiConfigGet();
    void handleApiConfigPost();
    void handleApiConfigSectionGet();
    void handleApiConfigSectionPost();
    void handleApiMQTTCaPost();
    void handleApiMQTTCaDelete();
    
    // API handlers - WiFi
    void handleApiWiFiStatus();
    void handleApiWiFiScan();
    
    // API handlers - MQTT
    void handleApiMQTTStatus();
    
    // API handlers - Time
    void handleApiTimeStatus();
    
    // API handlers - Errors
    void handleApiErrors();
    void handleApiErrorsClear();
    
    // API handlers - System
    void handleApiRestart();
    void handleApiReset();
    void handleApiInfo();

    // API handlers - Pump (motor_* firmware)
    void handleApiPumpGet();
    void handleApiPumpPost();

    void handleFirmwareUpload();
    void handleFirmwareUploadComplete();

    // Helper methods
    void sendJson(int code, const String& json);
    void sendError(int code, const String& message);
    /** Stable API error: { "success":false, "code": "...", "message": "..." } */
    void sendApiFailure(int httpCode, const char* codeStr, const String& message);
    void sendSuccess(const String& message);
    void addCorsHeaders();
    bool checkAuth();
    
    // Member variables
    ESP8266WebServer _server;       ///< Web server instance
    ISensor& _sensor;               ///< Sensor reference (ISensor interface)
    TankCalculator& _calculator;    ///< Calculator reference
    bool _running;                  ///< Server running flag
    uint32_t _requestCount;         ///< Request counter
    bool _firmwareUploadOk;         ///< Last web OTA result
    FlmOtaPrepareFn _otaPrepare;
    FlmOtaRestoreFn _otaRestore;
    IMotorController* _motor;     ///< nullptr on sensor_only builds
};

#endif // WEB_SERVER_H

