#include "unity.h"

// ---- Stub definitions ----
// Basic types and macros from ESP-IDF/FreeRTOS needed to compile mqtt_manager.cpp

typedef void* esp_mqtt_client_handle_t;
typedef int esp_mqtt_event_id_t;
typedef struct esp_mqtt_event {
    esp_mqtt_event_id_t event_id;
} esp_mqtt_event_t;
typedef esp_mqtt_event_t* esp_mqtt_event_handle_t;
typedef const char* esp_event_base_t;

// MQTT event IDs used in mqtt_manager.cpp
enum {
    MQTT_EVENT_CONNECTED = 0,
    MQTT_EVENT_DISCONNECTED,
    MQTT_EVENT_SUBSCRIBED,
    MQTT_EVENT_UNSUBSCRIBED,
    MQTT_EVENT_PUBLISHED,
    MQTT_EVENT_DATA,
    MQTT_EVENT_ERROR
};

// Logging stubs
#define ESP_LOGI(tag, fmt, ...)
#define ESP_LOGE(tag, fmt, ...)
#define ESP_LOGD(tag, fmt, ...)
#define ESP_LOGW(tag, fmt, ...)

// FreeRTOS stubs
typedef void* TaskHandle_t;
typedef void (*TaskFunction_t)(void*);
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
#define pdPASS 1
#define portTICK_PERIOD_MS 1
static inline void vTaskDelay(int) {}

// Replace task creation/deletion with test helpers
static int created_tasks = 0;
static inline BaseType_t test_xTaskCreate(TaskFunction_t task, const char* name, uint32_t stack, void* params, UBaseType_t prio, TaskHandle_t* handle) {
    created_tasks++;
    if (handle) {
        *handle = (TaskHandle_t)1; // dummy non-null handle
    }
    return pdPASS;
}
static inline void test_vTaskDelete(TaskHandle_t handle) {
    if (created_tasks > 0) {
        created_tasks--;
    }
}

#define xTaskCreate test_xTaskCreate
#define vTaskDelete test_vTaskDelete

// Other function stubs
static inline int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t, const char*, int) { return 0; }
static inline int esp_mqtt_client_publish(esp_mqtt_client_handle_t, const char*, const char*, int, int, int) { return 0; }
static inline uint32_t esp_log_timestamp(void) { return 0; }
static inline esp_mqtt_client_handle_t esp_mqtt_client_init(void*) { return NULL; }
static inline void esp_mqtt_client_register_event(esp_mqtt_client_handle_t, esp_mqtt_event_id_t, void*, void*) {}
static inline void esp_mqtt_client_start(esp_mqtt_client_handle_t) {}

// Include the implementation under test
#define static
#include "../main/mqtt_manager.cpp"
#undef static

TEST_CASE("publisher task starts only once on repeated MQTT_EVENT_CONNECTED", "[mqtt]")
{
    created_tasks = 0;
    publisher_task_handle = NULL; // ensure initial state

    esp_mqtt_event_t event = { .event_id = MQTT_EVENT_CONNECTED };

    // Simulate two connection events
    mqtt_event_handler(NULL, NULL, MQTT_EVENT_CONNECTED, &event);
    mqtt_event_handler(NULL, NULL, MQTT_EVENT_CONNECTED, &event);

    TEST_ASSERT_EQUAL_INT(1, created_tasks);
}
