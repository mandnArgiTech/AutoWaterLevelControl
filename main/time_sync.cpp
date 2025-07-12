#include "time_sync.h"
#include <sys/time.h>
#include "esp_sntp.h"
#include "esp_log.h"

static const char *TAG = "TIME_SYNC";

// Static variable to hold the event group handle passed from main
static EventGroupHandle_t s_time_event_group;

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronized");
    xEventGroupSetBits(s_time_event_group, TIME_SYNC_BIT);
}

void sntp_sync_time(EventGroupHandle_t time_event_group)
{
    s_time_event_group = time_event_group;

    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
}