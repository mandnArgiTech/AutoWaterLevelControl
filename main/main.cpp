#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <time.h>

#include "wifi_manager.h"
#include "time_sync.h"
#include "mqtt_manager.h"

static const char *TAG = "APP_MAIN";

/* FreeRTOS event group to signal when we are connected & time is synced */
static EventGroupHandle_t s_app_event_group;

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %ld bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
 
    // Initialize NVS - required for Wi-Fi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
 
    // Create an event group to manage connectivity status
    s_app_event_group = xEventGroupCreate();
 
    // Connect to Wi-Fi
    wifi_init_sta(s_app_event_group);
 
    // Wait for Wi-Fi connection before proceeding
    ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");
    xEventGroupWaitBits(s_app_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Wi-Fi Connected.");
 
    // Synchronize time from NTP server - required for TLS
    sntp_sync_time(s_app_event_group);
 
    // Start MQTT client
    ESP_LOGI(TAG, "Waiting for time synchronization...");
    xEventGroupWaitBits(s_app_event_group, TIME_SYNC_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Time synchronized.");
 
    // Set timezone to India Standard Time
    setenv("TZ", "IST-5:30", 1);
    tzset();
 
    // Start MQTT client
    ESP_LOGI(TAG, "Starting MQTT client...");
    mqtt_app_start();
    ESP_LOGI(TAG, "MQTT client started.");
}
