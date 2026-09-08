/**
 * @file main.cpp
 * @brief FluidLevelMonitor — sensor_only | motor_relay | motor_sms (see platformio.ini)
 */
#include <Arduino.h>

#include "version.h"
#include "config/ConfigManager.h"
#include "utils/ErrorHandler.h"
#include "utils/TimeManager.h"
#include "sensor/ISensor.h"
#include "sensor/SensorFactory.h"
#include "sensor/DHT11Ambient.h"
#include "tank/TankCalculator.h"
#include "network/WiFiManager.h"
#include "network/MQTTManager.h"
#include "network/WebServer.h"
#include "utils/Log.h"
#include "utils/FlmTime.h"
#include "utils/HeapMonitor.h"
#include "utils/BootDiagnostics.h"
#include "utils/BatteryMonitor.h"
#ifdef FLM_BATTERY_MONITOR
#include "utils/adc_scaling.h"
#endif

#if defined(FLM_ROLE_MOTOR_RELAY) || defined(FLM_ROLE_MOTOR_SMS)
#include "motor/MotorControllerFactory.h"
#include "motor/IMotorController.h"
#endif

ISensor*              activeSensor   = nullptr;
DHT11Ambient*         ambientSensor  = nullptr;
TankCalculator*       calculator     = nullptr;
WebServerManager*     webServer      = nullptr;

#if defined(FLM_ROLE_MOTOR_RELAY) || defined(FLM_ROLE_MOTOR_SMS)
IMotorController*     flmMotor       = nullptr;
#endif

unsigned long lastSensorRead   = 0;
unsigned long lastMQTTPublish  = 0;
unsigned long lastStatusPrint  = 0;
bool          webUiStarted     = false;
unsigned long bootMillis       = 0;

#define STATUS_PRINT_INTERVAL  10000
#define FLM_UI_DEFER_TIMEOUT_MS 45000u  ///< Start web UI even if MQTT TLS never connects

void initializeSystem();
void processLoop();
void readSensor();
void publishMQTT();
void printStatus();
void startWebUi();
void maybeStartWebUi();
void handleWiFiStateChange(WifiMgrState state);
void handleMQTTMessage(const String& topic, const String& payload);

void fluidPrepareForOTA() {
    MQTTManager::getInstance().prepareForOTA();
}

void fluidRestoreAfterOTA() {
    MQTTManager::getInstance().restoreAfterOTA();
}

void setup() {
    Serial.begin(115200);
    delay(100);
    bootMillis = millis();
    Version::printInfo();
    initializeSystem();
}

void initializeSystem() {
    Serial.println(F("\n========== System Initialization ==========\n"));

    Serial.println(F(">>> Configuration..."));
    ErrorCode result = ConfigManager::getInstance().begin();
    if (result != ErrorCode::ERR_NONE) {
        Serial.println(F("WARNING: config init had issues"));
    }

#ifdef FLM_BATTERY_MONITOR
    {
        const BatteryConfig& battCfg = ConfigManager::getInstance().getBatteryConfig();
        BatteryMonitor::getInstance().begin(battCfg.cellsInSeries);
        BatteryMonitor::getInstance().setCalibrationOffset(battCfg.calibrationOffset);
        Serial.printf("[Main] Battery A0: cells=%u offset=%.3fV scale=%.2f max=%.1fV\n",
                      (unsigned)battCfg.cellsInSeries,
                      battCfg.calibrationOffset,
                      adcBatteryVoltageRatio(),
                      adcMaxBatteryVoltage());
    }
#endif

    Serial.println(F("\n>>> Error Handler..."));
    ErrorHandler::getInstance().begin();

    Serial.println(F("\n>>> Boot diagnostics..."));
    BootDiagnostics::getInstance().begin();

    Serial.println(F("\n>>> Sensor..."));
    SensorConfig& sensorCfg = ConfigManager::getInstance().getSensorConfig();
    Serial.printf("[Main] Sensor type: %s\n", sensorCfg.hardware.type.c_str());

    activeSensor = SensorFactory::createSensor(sensorCfg.hardware);
    if (!activeSensor) {
        Serial.println(F("ERROR: factory returned nullptr — creating default A02YYUW"));
        SensorHWConfig defCfg;
        SensorFactory::getDefaultPins("A02YYUW", defCfg);
        defCfg.type = "A02YYUW";
        activeSensor = SensorFactory::createSensor(defCfg);
        if (!activeSensor) {
            Serial.println(F("FATAL: sensor allocation failed — halting"));
            while (true) { delay(1000); yield(); }
        }
    }

    result = activeSensor->begin();
    if (result != ErrorCode::ERR_NONE) {
        Serial.println(F("WARNING: sensor init failed"));
    } else {
        Serial.printf("[Main] Active: %s\n", activeSensor->getSensorTypeName().c_str());
    }

    // Additive calibration trim (same model as tmp/a02yyuw_sensor).
    // Tank math is: waterHeight = tankHeight - distance (distance already includes trim).
    activeSensor->setCalibrationOffset(sensorCfg.offsetMm);
    Serial.printf("[Main] Calibration offset: %.1f mm\n", sensorCfg.offsetMm);

    // Filters disabled: dashboard uses raw + calibration offset only.
    activeSensor->configureFilter(
        false,
        sensorCfg.medianFilterSize,
        sensorCfg.movingAvgWindow,
        false,
        sensorCfg.kalmanProcessNoise,
        sensorCfg.kalmanMeasureNoise);

    if (sensorCfg.ambient.enabled) {
        Serial.println(F("\n>>> Ambient sensor (DHT11)..."));
        ambientSensor = new DHT11Ambient(sensorCfg.ambient.pin);
        if (ambientSensor) {
            ambientSensor->setReadIntervalMs(sensorCfg.ambient.readIntervalMs);
            result = ambientSensor->begin();
            if (result != ErrorCode::ERR_NONE) {
                Serial.println(F("WARNING: DHT11 init had issues"));
            }
        }
    }

    Serial.println(F("\n>>> Tank Calculator..."));
    calculator = new TankCalculator(*activeSensor, ambientSensor);
    if (!calculator) {
        Serial.println(F("FATAL: calculator allocation failed — halting"));
        while (true) { delay(1000); yield(); }
    }
    result = calculator->begin();
    if (result != ErrorCode::ERR_NONE) {
        Serial.println(F("WARNING: tank calculator init had issues"));
    }

    Serial.println(F("\n>>> WiFi..."));
    WiFiManager::getInstance().setStateCallback(handleWiFiStateChange);
    result = WiFiManager::getInstance().begin();
    if (result != ErrorCode::ERR_NONE) {
        Serial.println(F("WARNING: WiFi init had issues"));
    }

    Serial.println(F("\n>>> Time Manager..."));
    TimeManager::getInstance().begin();

    Serial.println(F("\n>>> MQTT..."));
    MQTTManager::getInstance().setMessageCallback(handleMQTTMessage);
    result = MQTTManager::getInstance().begin();
    if (result == ErrorCode::ERR_MQTT_DISABLED) {
        Serial.println(F("INFO: MQTT disabled"));
    }

#if defined(FLM_ROLE_MOTOR_RELAY) || defined(FLM_ROLE_MOTOR_SMS)
    flmMotor = MotorControllerFactory::create();
    if (flmMotor) {
        MotorConfig& mc = ConfigManager::getInstance().getMotorConfig();
        flmMotor->begin(mc);
        Serial.println(F("[Main] Motor controller initialized (config relay section)"));
    }
#endif

    // Defer HTTP + Calibration WebSocket until MQTT TLS has connected (or timed out).
    // Starting them first leaves only ~14 KB free — below the BearSSL handshake budget.
    {
        MQTTConfig& mqttCfg = ConfigManager::getInstance().getMQTTConfig();
        const bool deferUi = mqttCfg.enabled && mqttCfg.tls;
        if (!deferUi) {
            startWebUi();
        } else {
            Serial.println(F("\n>>> Web UI deferred until MQTT TLS connects (saves heap)"));
            Serial.printf("[Main] Free heap now: %u (maxBlock %u)\n",
                          (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxFreeBlockSize());
        }
    }

    WiFiManager::getInstance().setOTAPrepareCallback([]() {
        fluidPrepareForOTA();
        if (webServer) webServer->stopForOTA();
    });
    WiFiManager::getInstance().setOTAErrorCallback([]() {
        if (webServer) webServer->resumeAfterOTA();
        fluidRestoreAfterOTA();
    });

    Serial.println(F("\n>>> Initial sensor reading..."));
    readSensor();

    Serial.println(F("\n========== Initialization Complete =========="));

    /* ESP8266: hardware WDT ~3.2s; wdtEnable(ms) is a no-op on core 3.x — feed in loop. */
    Serial.println(F("[Main] WDT fed each loop (~3.2s window on ESP8266)\n"));

    if (WiFiManager::getInstance().isConnected()) {
        if (webServer) {
            Serial.printf("Web:  http://%s/\n", WiFiManager::getInstance().getIP().c_str());
            Serial.printf("mDNS: http://%s.local/\n",
                          ConfigManager::getInstance().getWiFiConfig().hostname.c_str());
            Serial.printf("WS:   ws://%s:81/\n", WiFiManager::getInstance().getIP().c_str());
        } else {
            Serial.printf("IP:   %s (web UI starting after MQTT)\n",
                          WiFiManager::getInstance().getIP().c_str());
        }
    } else if (WiFiManager::getInstance().isAPMode()) {
        Serial.println(F("\n*** ACCESS POINT MODE ***"));
        Serial.printf("SSID: %s\n", WiFiManager::getInstance().getAPSSID().c_str());
        Serial.printf("Pass: %s\n", ConfigManager::getInstance().getWiFiConfig().apPassword.c_str());
        Serial.printf("URL:  http://%s\n", WiFiManager::getInstance().getAPIP().c_str());
        startWebUi();  // AP mode needs the config UI immediately
    }
    Serial.println(F("\n================================================\n"));
}

void loop() {
    processLoop();
}

void processLoop() {
    HeapMonitor::sample();
    BootDiagnostics::getInstance().loop();

    // Service HTTP first — ESP8266WebServer is single-client; delays here stall the UI.
    if (webServer) webServer->loop();

    WiFiManager::getInstance().loop();
    if (WiFiManager::getInstance().isOTAInProgress()) return;

    ESP.wdtFeed();
    if (webServer) webServer->loop();

    TimeManager::getInstance().loop();
    MQTTManager::getInstance().loop();
    maybeStartWebUi();

    if (webServer) webServer->loop();

    if (activeSensor) activeSensor->poll();

#if defined(FLM_ROLE_MOTOR_RELAY) || defined(FLM_ROLE_MOTOR_SMS)
    float motorPct = -1.f;
    const WaterLevel& lv = calculator->getLastLevel();
    if (lv.sensorOk && lv.valid) motorPct = lv.percentFilled;
    if (flmMotor) flmMotor->loop(motorPct);
#endif

    {
        uint32_t interval = ConfigManager::getInstance().getSensorConfig().readInterval;
        if (flmElapsedMs(lastSensorRead, interval)) {
            readSensor();
            if (webServer) webServer->loop();
        }
    }

    MQTTConfig& mqttCfg = ConfigManager::getInstance().getMQTTConfig();
    if (mqttCfg.enabled && MQTTManager::getInstance().isConnected()) {
        if (flmElapsedMs(lastMQTTPublish, mqttCfg.publishInterval)) {
            // Shed MQTT publish when heap is critically low (avoids OOM reboot spiral).
            if (HeapMonitor::belowFloor()) {
                static unsigned long lastHeapWarn = 0;
                if (millis() - lastHeapWarn >= 10000UL) {
                    lastHeapWarn = millis();
                    FLM_LOG_WARN("Main", "heap %u < floor %u — skip MQTT publish",
                                 (unsigned)HeapMonitor::freeHeap(),
                                 (unsigned)FLM_HEAP_FLOOR_BYTES);
                }
            } else {
                publishMQTT();
            }
            if (webServer) webServer->loop();
        }
    }

    if (ConfigManager::getInstance().getSystemConfig().debugEnabled) {
        if (flmElapsedMs(lastStatusPrint, STATUS_PRINT_INTERVAL)) {
            printStatus();
        }
    }

    yield();
    if (webServer) webServer->loop();

#ifdef FLM_DEEP_SLEEP_ENABLED
    // Deep sleep mode: after publishing, sleep until next cycle
    static bool hasSentFirstReading = false;
    MQTTConfig& mqttCfgSleep = ConfigManager::getInstance().getMQTTConfig();
    if (mqttCfgSleep.enabled && MQTTManager::getInstance().isConnected() && hasSentFirstReading) {
        float bv = BatteryMonitor::getInstance().readVoltage();
        uint32_t sleepSec = FLM_SLEEP_SECONDS;
        if (bv < BATT_VOLTAGE_CRITICAL) sleepSec = FLM_SLEEP_CRITICAL;
        else if (bv < BATT_VOLTAGE_LOW) sleepSec = FLM_SLEEP_LOW_BATT;
        FLM_LOG_INFO("Main", "Deep sleep %us (batt %.2fV)", sleepSec, bv);
        MQTTManager::getInstance().disconnect();
        WiFiManager::getInstance().disconnect();
        delay(200);
        ESP.deepSleep((uint64_t)sleepSec * 1000000UL);
    }
    hasSentFirstReading = true;
#endif
}

void readSensor() {
    lastSensorRead = millis();
    WaterLevel level = calculator->calculate();

    // Always log sensor failures — A02YYUW "no_frame"/wiring issues are otherwise silent.
    static unsigned long lastSensorMsg = 0;
    if (!level.sensorOk) {
        if (millis() - lastSensorMsg >= 5000) {
            lastSensorMsg = millis();
            Serial.printf("[Main] Sensor FAIL err=%d — A02YYUW frames OK but distance rejected (often below 30mm blind zone)\n",
                          (int)level.error);
            if (activeSensor) {
                Serial.println(activeSensor->getStatusJson());
            }
        }
        return;
    }

    SystemConfig& cfg = ConfigManager::getInstance().getSystemConfig();
    if (!cfg.debugEnabled) return;

    if (level.valid) {
        Serial.printf("[Main] %.1f%% filled  Height: %.1f cm  Dist: %.1f cm\n",
                      level.percentFilled, level.waterHeightCm, level.distanceCm);
        if (level.temperatureValid) {
            Serial.printf("[Main] Temp: %.1f C", level.temperatureC);
            if (level.humidityValid) {
                Serial.printf("  Humidity: %.1f%%", level.humidityPct);
            }
            Serial.println();
        }
    }
}

void publishMQTT() {
    if (!MQTTManager::getInstance().isConnected()) return;

    String ts  = TimeManager::getInstance().getISO8601();
    String json = calculator->getMQTTJson(ts);

#ifdef FLM_BATTERY_MONITOR
    // Slim battery fields only — full getJson() diagnostics blow past MQTT buffer.
    {
        JsonDocument doc;
        if (deserializeJson(doc, json) == DeserializationError::Ok) {
            const float v = BatteryMonitor::getInstance().readVoltageCached(3000);
            JsonObject batt = doc["battery"].to<JsonObject>();
            batt["voltage"] = roundf(v * 100.0f) / 100.0f;
            batt["percent"] = BatteryMonitor::getInstance().getStateOfCharge(v);
            json = "";
            serializeJson(doc, json);
        }
    }
#endif

    bool ok = MQTTManager::getInstance().publishWaterLevel(json);

    if (ConfigManager::getInstance().getSystemConfig().debugEnabled) {
        if (ok) {
            const WaterLevel& last = calculator->getLastLevel();
            Serial.println(last.sensorOk ? F("[Main] MQTT published")
                                         : F("[Main] MQTT published (sensor not responding)"));
        } else {
            Serial.printf("[Main] MQTT publish failed (payload %u bytes)\n",
                          (unsigned)json.length());
        }
    }
}

void printStatus() {
    const WaterLevel& level = calculator->getLastLevel();

    Serial.println(F("--- Status ---"));
    Serial.printf("Sensor: %s | %s\n",
                  activeSensor->getSensorTypeName().c_str(),
                  level.sensorOk ? "OK" : "NOT RESPONDING");

    if (level.sensorOk && level.valid) {
        Serial.printf("Filled: %.1f%% | Remaining: %.1f%%\n",
                      level.percentFilled, level.percentRemaining);
        Serial.printf("Height: %.1f cm | Volume: %.1f L\n",
                      level.waterHeightCm, level.volumeLiters);
        Serial.printf("State: %s\n",
                      TankCalculator::tankStateToString(calculator->getTankState()).c_str());
    } else if (!level.sensorOk) {
        Serial.printf("Error: %s\n",
                      ErrorHandler::getInstance().getErrorDescription(level.error).c_str());
    }

    if (level.temperatureValid) {
        Serial.printf("Ambient: %.1f C", level.temperatureC);
        if (level.humidityValid) {
            Serial.printf("  %.1f%% RH", level.humidityPct);
        }
        Serial.println();
    }

    Serial.printf("WiFi: %s | MQTT: %s\n",
                  WiFiManager::getInstance().isConnected()     ? "Connected" : "Disconnected",
                  MQTTManager::getInstance().isConnected()     ? "Connected" : "Disconnected");
    Serial.printf("Uptime: %s | Heap: %u bytes\n",
                  TimeManager::getInstance().getUptimeString().c_str(),
                  ESP.getFreeHeap());
    Serial.println(F("--------------\n"));
}

void startWebUi() {
    if (webUiStarted) return;
    webUiStarted = true;

    Serial.println(F("\n>>> Web Server..."));
#if defined(FLM_ROLE_MOTOR_RELAY) || defined(FLM_ROLE_MOTOR_SMS)
    webServer = new WebServerManager(*activeSensor, *calculator, fluidPrepareForOTA,
                                     fluidRestoreAfterOTA, flmMotor);
#else
    webServer = new WebServerManager(*activeSensor, *calculator, fluidPrepareForOTA,
                                     fluidRestoreAfterOTA, nullptr);
#endif
    if (webServer) webServer->begin();

    if (WiFiManager::getInstance().isConnected()) {
        Serial.printf("Web:  http://%s/\n", WiFiManager::getInstance().getIP().c_str());
    }
    Serial.printf("[Main] UI up — heap %u maxBlock %u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxFreeBlockSize());
}

void maybeStartWebUi() {
    if (webUiStarted) return;

    MQTTConfig& mqttCfg = ConfigManager::getInstance().getMQTTConfig();
    const bool mqttDone = !mqttCfg.enabled || !mqttCfg.tls
                          || MQTTManager::getInstance().isConnected();
    const bool timedOut = (millis() - bootMillis) >= FLM_UI_DEFER_TIMEOUT_MS;

    if (mqttDone || timedOut) {
        if (timedOut && !mqttDone) {
            Serial.println(F("[Main] MQTT TLS still down — starting web UI anyway"));
        }
        startWebUi();
    }
}

void handleWiFiStateChange(WifiMgrState state) {
    FLM_LOG_INFO("Main", "WiFi -> %s", WiFiManager::stateToString(state).c_str());

    if (state == WifiMgrState::CONNECTED) {
        TimeManager::getInstance().syncTime();
        if (MQTTManager::getInstance().isEnabled()) {
            MQTTManager::getInstance().connect();
        }
        maybeStartWebUi();
    }
}

void handleMQTTMessage(const String& topic, const String& payload) {
    FLM_LOG_DEBUG("Main", "MQTT %s", topic.c_str());

    if (!topic.endsWith("/command")) return;

    JsonDocument doc;
    if (deserializeJson(doc, payload)) return;

    String cmd = doc["command"] | "";
    if (cmd == "read") {
        readSensor();
        publishMQTT();
    } else if (cmd == "status") {
        MQTTManager::getInstance().publishStatus();
    } else if (cmd == "restart") {
        Serial.println(F("[Main] Restarting..."));
        BootDiagnostics::getInstance().markIntentionalRestart("mqtt_restart");
        delay(500);
        ESP.restart();
#if defined(FLM_ROLE_MOTOR_RELAY) || defined(FLM_ROLE_MOTOR_SMS)
    } else if (flmMotor && (cmd == "pump_on" || cmd == "pump_off" || cmd == "pump_auto")) {
        if (cmd == "pump_on") flmMotor->setMode(MotorMode::ON);
        else if (cmd == "pump_off") flmMotor->setMode(MotorMode::OFF);
        else flmMotor->setMode(MotorMode::AUTO);
        MQTTManager::getInstance().publish("motor/status", flmMotor->getMQTTJson(), false);
#endif
    }
}
