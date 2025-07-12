#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Event group bits
#define TIME_SYNC_BIT      BIT1

/**
 * @brief Initializes SNTP to synchronize the system time.
 *
 * @param time_event_group The event group handle to signal time sync events.
 */
void sntp_sync_time(EventGroupHandle_t time_event_group);