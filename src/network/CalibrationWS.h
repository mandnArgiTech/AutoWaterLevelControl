/**
 * @file CalibrationWS.h
 * @brief WebSocket server for real-time sensor calibration
 * 
 * This module provides:
 * - WebSocket server for live sensor readings
 * - Real-time distance display in centimeters
 * - Calibration offset adjustment
 * - Save calibration to configuration
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#ifndef CALIBRATION_WS_H
#define CALIBRATION_WS_H

#include <Arduino.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "../sensor/ISensor.h"
#include "../config/ConfigManager.h"
#include "../tank/TankCalculator.h"

// =============================================================================
// SECTION 1: CONSTANTS
// =============================================================================

#define CALIBRATION_WS_PORT     81      ///< WebSocket port
#define CALIBRATION_INTERVAL    200     ///< Reading interval in ms (5 per second)

// =============================================================================
// SECTION 2: CALIBRATION WEBSOCKET CLASS
// =============================================================================

/**
 * @class CalibrationWebSocket
 * @brief WebSocket server for sensor calibration
 * 
 * Provides real-time sensor readings via WebSocket for calibration.
 * All distance values are displayed in centimeters.
 */
class CalibrationWebSocket {
public:
    /**
     * @brief Constructor with sensor reference
     * @param sensor Reference to ISensor
     * @param calculator Reference to TankCalculator
     */
    CalibrationWebSocket(ISensor& sensor, TankCalculator& calculator);
    
    /**
     * @brief Initialize WebSocket server
     * @return true if successful
     */
    bool begin();

    void stopForOTA();
    void resumeAfterOTA();

    /**
     * @brief Process WebSocket events (call in loop)
     */
    void loop();
    
    /**
     * @brief Check if calibration mode is active
     * @return true if clients connected
     */
    bool isActive() const { return _clientCount > 0; }
    
    /**
     * @brief Get connected client count
     * @return Number of connected clients
     */
    uint8_t getClientCount() const { return _clientCount; }
    
    /**
     * @brief Broadcast current sensor reading to all clients
     */
    void broadcastReading();
    
    /**
     * @brief Set calibration offset
     * @param offsetCm Offset in centimeters
     */
    void setCalibrationOffset(float offsetCm);
    
    /**
     * @brief Get current calibration offset in cm
     * @return Offset in centimeters
     */
    float getCalibrationOffsetCm() const;

private:
    // WebSocket event handler
    void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
    
    // Handle incoming commands
    void handleCommand(uint8_t num, const uint8_t* payload, size_t length);
    
    // Send JSON to client
    void sendToClient(uint8_t num, const String& json);
    void broadcastJson(const String& json);
    
    // Generate reading JSON
    String getReadingJson();
    
    // Generate status JSON
    String getStatusJson();
    
    // Member variables
    WebSocketsServer _wsServer;         ///< WebSocket server
    ISensor& _sensor;                   ///< Sensor reference
    TankCalculator& _calculator;        ///< Calculator reference
    uint8_t _clientCount;               ///< Connected clients
    unsigned long _lastBroadcast;       ///< Last broadcast time
    bool _initialized;                  ///< Init flag
    bool _suspendedForOTA;              ///< Stopped for OTA / web upload
    
    // Static instance for callback
    static CalibrationWebSocket* _instance;
    static void webSocketEventCallback(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
};

#endif // CALIBRATION_WS_H

