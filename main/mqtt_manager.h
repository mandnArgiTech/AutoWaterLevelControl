#pragma once

/**
 * @brief Initializes and starts the MQTT client.
 *
 * The client will attempt to connect to the broker and handle events
 * like connection, disconnection, and incoming messages.
 */
void mqtt_app_start(void);