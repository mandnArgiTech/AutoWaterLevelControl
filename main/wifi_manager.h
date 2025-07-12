#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Event group bits
#define WIFI_CONNECTED_BIT BIT0

/**
 * @brief Initializes the Wi-Fi station, connects to the configured AP,
 *        and signals connection status via an event group.
 *
 * @param wifi_event_group The event group handle to signal Wi-Fi events.
 */
void wifi_init_sta(EventGroupHandle_t wifi_event_group);