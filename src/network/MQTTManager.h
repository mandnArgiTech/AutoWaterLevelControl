/**
 * @file MQTTManager.h
 * @brief MQTT client for publishing water level data
 * 
 * Supports:
 * - Device-scoped topics: {deviceName}_{chipId}/{prefix}/{subtopic}
 * - Auto-reconnect, LWT, JSON publishing
 * 
 * @author FluidLevelMonitor Project
 * @version 1.2.0
 */

#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <functional>
#include "../config/ConfigManager.h"
#include "../utils/ErrorHandler.h"

#define MQTT_BUFFER_SIZE        512
#define MQTT_RECONNECT_INTERVAL 5000
#ifndef MQTT_KEEPALIVE
#define MQTT_KEEPALIVE          60
#endif

enum class MQTTState {
    DISABLED,
    DISCONNECTED,
    CONNECTING,
    CONNECTED
};

typedef std::function<void(const String& topic, const String& payload)> MQTTMessageCallback;
typedef std::function<void(MQTTState state)> MQTTStateCallback;

class MQTTManager {
public:
    static MQTTManager& getInstance();

    ErrorCode begin();
    void loop();
    ErrorCode connect();
    void disconnect();
    bool isConnected();
    bool isEnabled() const;
    MQTTState getState() const { return _state; }

    bool publishWaterLevel(const String& json);
    bool publish(const String& subtopic, const String& payload, bool retained = false);
    bool publishStatus();
    bool subscribe(const String& subtopic);

    void setMessageCallback(MQTTMessageCallback callback);
    void setStateCallback(MQTTStateCallback callback);

    String getTopic(const String& subtopic) const;
    String getDeviceTag() const { return _deviceTag; }

    String getStatusJson();
    static String stateToString(MQTTState state);

private:
    MQTTManager();
    MQTTManager(const MQTTManager&) = delete;
    MQTTManager& operator=(const MQTTManager&) = delete;

    static void mqttCallback(char* topic, byte* payload, unsigned int length);
    void handleMessage(const String& topic, const String& payload);
    void setState(MQTTState newState);
    void checkConnection();
    void buildDeviceTag();

    WiFiClient _wifiClient;
    PubSubClient _mqttClient;

    bool _initialized;
    MQTTState _state;
    String _deviceTag;
    String _uniqueClientId;

    MQTTMessageCallback _messageCallback;
    MQTTStateCallback _stateCallback;
    unsigned long _lastConnectAttempt;
    uint32_t _publishCount;
    uint32_t _publishErrors;

    static MQTTManager* _instance;
};

#endif // MQTT_MANAGER_H
