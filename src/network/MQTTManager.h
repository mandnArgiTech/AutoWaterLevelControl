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
#include <WiFiClientSecure.h>
#include <functional>
#include "../config/ConfigManager.h"
#include "../utils/ErrorHandler.h"

#define MQTT_BUFFER_SIZE        1024     // level JSON + battery can exceed 512
#define MQTT_RECONNECT_INTERVAL 5000
#define MQTT_RECONNECT_MAX_MS   30000u   // 30s cap — pump control needs fast reconnect
// PubSubClient defines MQTT_KEEPALIVE (15s) before this header can, so use our own name
#define FLM_MQTT_KEEPALIVE_S    60

// ---------------------------------------------------------------------------
// TLS (BearSSL) tuning for ESP8266.
//
// Default BearSSL buffers are 16 KB — a handshake then needs ~27-30 KB of
// contiguous heap, which this firmware does not have → OOM → reboot loop.
// With MFLN (Maximum Fragment Length Negotiation, supported by any
// OpenSSL 1.1.1+ broker such as Mosquitto on a modern VPS) buffers shrink
// to 1 KB each and a full handshake fits in ~13 KB peak.
// ---------------------------------------------------------------------------
#define FLM_TLS_RX_BUF          1024     ///< BearSSL receive buffer (needs broker MFLN)
#define FLM_TLS_TX_BUF          1024     ///< BearSSL transmit buffer
/** Peak handshake with 1 KB MFLN buffers is ~12–14 KB; keep a small margin. */
#define FLM_TLS_MIN_FREE_HEAP   14000u
#define FLM_TLS_MIN_MAX_BLOCK   10000u
#define MQTT_CA_FILE            "/mqtt_ca.pem"  ///< LittleFS path for CA cert (tlsMode=ca)

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
    /** Disconnect and shrink buffer before ArduinoOTA (free heap). */
    void prepareForOTA();
    /** Restore MQTT buffer after failed OTA; loop() will reconnect. */
    void restoreAfterOTA();
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
    /** Create + configure the TLS client per config. Returns false on fatal config error. */
    bool setupTls(const MQTTConfig& config);
    /** Tear down TLS client objects (reclaim heap after a failed handshake). */
    void teardownTlsClient();
    /** True when TLS preconditions (heap, NTP time for cert validation) are met. */
    bool tlsReady();
    /** Log BearSSL / heap diagnostics after a failed connect. */
    void logTlsFailure();

    WiFiClient _wifiClient;
    BearSSL::WiFiClientSecure* _secureClient;  // allocated only when TLS enabled
    BearSSL::X509List* _caCerts;               // parsed CA cert (tlsMode=ca)
    BearSSL::Session* _tlsSession;             // TLS session cache — fast resumed handshakes
    bool _useTls;
    bool _tlsNeedsTime;                        // cert validation requires synced clock
    PubSubClient _mqttClient;

    bool _initialized;
    MQTTState _state;
    String _deviceTag;
    String _uniqueClientId;

    MQTTMessageCallback _messageCallback;
    MQTTStateCallback _stateCallback;
    unsigned long _lastConnectAttempt;
    uint32_t _reconnectDelayMs;
    uint32_t _publishCount;
    uint32_t _publishErrors;

    static MQTTManager* _instance;
};

#endif // MQTT_MANAGER_H
