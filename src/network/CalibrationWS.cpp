/**
 * @file CalibrationWS.cpp
 * @brief Implementation of WebSocket server for calibration
 * 
 * @author FluidLevelMonitor Project
 * @version 1.0.0
 */

#include "CalibrationWS.h"

// Static instance for callback
CalibrationWebSocket* CalibrationWebSocket::_instance = nullptr;

// =============================================================================
// SECTION 1: CONSTRUCTOR
// =============================================================================

/**
 * @brief Constructor with sensor and calculator references
 */
CalibrationWebSocket::CalibrationWebSocket(ISensor& sensor, TankCalculator& calculator)
    : _wsServer(CALIBRATION_WS_PORT)
    , _sensor(sensor)
    , _calculator(calculator)
    , _clientCount(0)
    , _lastBroadcast(0)
    , _initialized(false)
    , _suspendedForOTA(false) {

    _instance = this;
}

// =============================================================================
// SECTION 2: INITIALIZATION
// =============================================================================

/**
 * @brief Initialize WebSocket server
 * @return true if successful
 */
bool CalibrationWebSocket::begin() {
    Serial.println(F("[CalibrationWS] Initializing..."));
    
    // Step 1: Start WebSocket server
    _wsServer.begin();
    
    // Step 2: Set event handler
    _wsServer.onEvent(webSocketEventCallback);
    
    _initialized = true;
    Serial.printf("[CalibrationWS] WebSocket server started on port %d\n", CALIBRATION_WS_PORT);
    
    return true;
}

// =============================================================================
// SECTION 3: LOOP PROCESSING
// =============================================================================

/**
 * @brief Process WebSocket events
 */
void CalibrationWebSocket::stopForOTA() {
    if (!_initialized || _suspendedForOTA) return;
    _wsServer.close();
    _suspendedForOTA = true;
    _clientCount = 0;
    Serial.println(F("[CalibrationWS] Stopped for OTA"));
}

void CalibrationWebSocket::resumeAfterOTA() {
    if (!_suspendedForOTA) return;
    _wsServer.begin();
    _wsServer.onEvent(webSocketEventCallback);
    _suspendedForOTA = false;
    Serial.println(F("[CalibrationWS] Resumed"));
}

void CalibrationWebSocket::loop() {
    if (!_initialized || _suspendedForOTA) return;
    
    // Step 1: Process WebSocket
    _wsServer.loop();
    
    // Step 2: Broadcast readings if clients connected
    if (_clientCount > 0) {
        if (millis() - _lastBroadcast >= CALIBRATION_INTERVAL) {
            broadcastReading();
            _lastBroadcast = millis();
        }
    }
}

// =============================================================================
// SECTION 4: WEBSOCKET EVENTS
// =============================================================================

/**
 * @brief Static callback for WebSocket events
 */
void CalibrationWebSocket::webSocketEventCallback(uint8_t num, WStype_t type, 
                                                   uint8_t* payload, size_t length) {
    if (_instance) {
        _instance->onWebSocketEvent(num, type, payload, length);
    }
}

/**
 * @brief Handle WebSocket events
 */
void CalibrationWebSocket::onWebSocketEvent(uint8_t num, WStype_t type, 
                                             uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.printf("[CalibrationWS] Client %u disconnected\n", num);
            if (_clientCount > 0) _clientCount--;
            break;
            
        case WStype_CONNECTED:
            {
                Serial.printf("[CalibrationWS] Client %u connected\n", num);
                _clientCount++;
                
                // Send initial status
                sendToClient(num, getStatusJson());
            }
            break;
            
        case WStype_TEXT:
            handleCommand(num, payload, length);
            break;
            
        default:
            break;
    }
}

// =============================================================================
// SECTION 5: COMMAND HANDLING
// =============================================================================

/**
 * @brief Handle incoming commands from client
 */
void CalibrationWebSocket::handleCommand(uint8_t num, const uint8_t* payload, size_t length) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    
    if (error) {
        Serial.println(F("[CalibrationWS] JSON parse error"));
        return;
    }
    
    String cmd = doc["cmd"] | "";
    
    if (cmd == "getStatus") {
        // Send current status
        sendToClient(num, getStatusJson());
        
    } else if (cmd == "getReading") {
        // Send single reading
        sendToClient(num, getReadingJson());
        
    } else if (cmd == "setOffset") {
        // Set calibration offset (in cm)
        float offsetCm = doc["offset"] | 0.0f;
        setCalibrationOffset(offsetCm);
        
        // Confirm with status
        broadcastJson(getStatusJson());
        Serial.printf("[CalibrationWS] Offset set to %.1f cm\n", offsetCm);
        
    } else if (cmd == "saveCalibration") {
        // Save calibration to config
        SensorConfig& sensorConfig = ConfigManager::getInstance().getSensorConfig();
        sensorConfig.offsetMm = _sensor.getCalibrationOffset();
        
        ErrorCode result = ConfigManager::getInstance().saveConfig();
        
        char buf[96];
        JsonDocument response;
        response["type"] = "saveResult";
        response["success"] = (result == ErrorCode::ERR_NONE);
        response["offsetCm"] = sensorConfig.offsetMm / 10.0f;
        size_t n = serializeJson(response, buf, sizeof(buf));
        if (n > 0 && n < sizeof(buf)) {
            _wsServer.sendTXT(num, (uint8_t*)buf, n);
        }
        
        Serial.printf("[CalibrationWS] Calibration saved: %.1f cm\n", sensorConfig.offsetMm / 10.0f);
        
    } else if (cmd == "resetOffset") {
        // Reset offset to zero
        setCalibrationOffset(0);
        broadcastJson(getStatusJson());
        Serial.println(F("[CalibrationWS] Offset reset to 0"));
    }
}

// =============================================================================
// SECTION 6: BROADCASTING
// =============================================================================

/**
 * @brief Broadcast current reading to all clients
 */
void CalibrationWebSocket::broadcastReading() {
    if (_clientCount == 0) return;
    broadcastJson(getReadingJson());
}

/**
 * @brief Set calibration offset in cm
 */
void CalibrationWebSocket::setCalibrationOffset(float offsetCm) {
    _sensor.setCalibrationOffset(offsetCm * 10.0f);  // Convert to mm
}

/**
 * @brief Get calibration offset in cm
 */
float CalibrationWebSocket::getCalibrationOffsetCm() const {
    return _sensor.getCalibrationOffset() / 10.0f;
}

// =============================================================================
// SECTION 7: JSON GENERATION
// =============================================================================

/**
 * @brief Generate reading JSON
 */
String CalibrationWebSocket::getReadingJson() {
    SensorReading reading = _sensor.getReading();
    
    JsonDocument doc;
    doc["type"] = "reading";
    doc["valid"] = reading.distanceValid;
    doc["sensorOk"] = reading.distanceValid;
    doc["timestamp"] = millis();
    doc["calibrationOffsetCm"] = round(getCalibrationOffsetCm() * 10) / 10.0;
    
    if (reading.distanceValid) {
        doc["distanceCm"] = round(reading.distanceCm * 10) / 10.0;
        doc["distanceMm"] = round(reading.distanceMm);
        if (reading.temperatureValid) {
            doc["temperatureC"] = reading.temperatureC;
        }
        
        WaterLevel level = _calculator.calculateFromDistance(reading.distanceMm);
        doc["waterHeightCm"] = round(level.waterHeightCm * 10) / 10.0;
        doc["percentFilled"] = round(level.percentFilled * 10) / 10.0;
        doc["percentRemaining"] = round(level.percentRemaining * 10) / 10.0;
    } else {
        doc["sensorStatus"] = "not_responding";
        doc["errorCode"] = static_cast<uint16_t>(reading.lastError);
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

/**
 * @brief Generate status JSON
 */
String CalibrationWebSocket::getStatusJson() {
    TankConfig& tankConfig = ConfigManager::getInstance().getTankConfig();
    SensorConfig& sensorConfig = ConfigManager::getInstance().getSensorConfig();
    
    JsonDocument doc;
    doc["type"] = "status";
    
    // Sensor info
    JsonObject sensor = doc["sensor"].to<JsonObject>();
    sensor["type"] = _sensor.getSensorTypeName();
    sensor["ready"] = _sensor.isReady();
    sensor["minRangeCm"] = _sensor.getMinRange() / 10.0f;
    sensor["maxRangeCm"] = _sensor.getMaxRange() / 10.0f;
    sensor["calibrationOffsetCm"] = sensorConfig.offsetMm / 10.0f;
    
    // Tank info
    JsonObject tank = doc["tank"].to<JsonObject>();
    tank["type"] = tankConfig.type;
    tank["heightCm"] = tankConfig.height / 10.0f;
    if (tankConfig.type == "circular") {
        tank["diameterCm"] = tankConfig.diameter / 10.0f;
    }
    tank["volumeLiters"] = tankConfig.volumeLiters;
    
    // Current offset
    doc["currentOffsetCm"] = getCalibrationOffsetCm();
    
    String output;
    serializeJson(doc, output);
    return output;
}

// =============================================================================
// SECTION 8: SEND HELPERS
// =============================================================================

/**
 * @brief Send JSON to specific client
 */
void CalibrationWebSocket::sendToClient(uint8_t num, const String& json) {
    if (json.length() == 0) return;
    _wsServer.sendTXT(num, json.c_str(), json.length());
}

void CalibrationWebSocket::broadcastJson(const String& json) {
    if (json.length() == 0) return;
    _wsServer.broadcastTXT(json.c_str(), json.length());
}

