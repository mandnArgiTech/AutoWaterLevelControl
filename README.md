# AutoWaterLevelControl

An ESP32-C6 based firmware for an automatic water level controller. This project connects to a Wi-Fi network, synchronizes time, and communicates with an MQTT broker for status updates and remote commands.

## Features

-   **Wi-Fi Connectivity**: Connects to a specified Wi-Fi network and automatically reconnects on disconnection.
-   **Secure MQTT Communication**: Uses TLS to establish a secure connection with an MQTT broker.
-   **Time Synchronization**: Synchronizes with an NTP server to get the correct time, which is crucial for TLS certificate validation.
-   **Modular Design**: The code is organized into logical modules for Wi-Fi, SNTP, and MQTT management, improving readability and maintainability.
-   **Status Publishing**: Periodically publishes status messages (e.g., uptime) to a defined MQTT topic.
-   **Remote Commands**: Subscribes to an MQTT topic to listen for incoming commands.

## Hardware Requirements

-   An ESP32-C6 development board.
-   Access to a Wi-Fi network.
-   An MQTT broker (e.g., Mosquitto) configured for TLS connections.

## Software Setup

1.  **ESP-IDF**: This project is built using ESP-IDF v5.3. Make sure you have it installed and configured correctly.
2.  **Clone the Repository**:
    ```bash
    git clone <your-repository-url>
    cd AutoWaterLevelControl
    ```
3.  **Certificates**: This project requires TLS certificates to connect to the MQTT broker.
    -   Place your CA certificate, client certificate, and client private key into the `main/certs/` directory.
    -   The files must be named `ca.crt`, `client.crt`, and `client.key`.

## Configuration

Project-specific configurations are currently hardcoded in the source files.

-   **Wi-Fi Credentials**: Modify `WIFI_SSID` and `WIFI_PASSWORD` in `main/wifi_manager.cpp`.
-   **MQTT Configuration**: Modify the broker URI, client ID, credentials, and topics in `main/mqtt_manager.cpp`.

## Project Structure

The project is organized into several components for clarity and separation of concerns.
# AutoWaterLevelControl

An ESP32-C6 based firmware for an automatic water level controller. This project connects to a Wi-Fi network, synchronizes time, and communicates with an MQTT broker for status updates and remote commands.

## Features

-   **Wi-Fi Connectivity**: Connects to a specified Wi-Fi network and automatically reconnects on disconnection.
-   **Secure MQTT Communication**: Uses TLS to establish a secure connection with an MQTT broker.
-   **Time Synchronization**: Synchronizes with an NTP server to get the correct time, which is crucial for TLS certificate validation.
-   **Modular Design**: The code is organized into logical modules for Wi-Fi, SNTP, and MQTT management, improving readability and maintainability.
-   **Status Publishing**: Periodically publishes status messages (e.g., uptime) to a defined MQTT topic.
-   **Remote Commands**: Subscribes to an MQTT topic to listen for incoming commands.

## Hardware Requirements

-   An ESP32-C6 development board.
-   Access to a Wi-Fi network.
-   An MQTT broker (e.g., Mosquitto) configured for TLS connections.

## Software Setup

1.  **ESP-IDF**: This project is built using ESP-IDF v5.3. Make sure you have it installed and configured correctly.
2.  **Clone the Repository**:
    ```bash
    git clone <your-repository-url>
    cd AutoWaterLevelControl
    ```
3.  **Certificates**: This project requires TLS certificates to connect to the MQTT broker.
    -   Place your CA certificate, client certificate, and client private key into the `main/certs/` directory.
    -   The files must be named `ca.crt`, `client.crt`, and `client.key`.

## Configuration

Project-specific configurations are currently hardcoded in the source files.

-   **Wi-Fi Credentials**: Modify `WIFI_SSID` and `WIFI_PASSWORD` in `main/wifi_manager.cpp`.
-   **MQTT Configuration**: Modify the broker URI, client ID, credentials, and topics in `main/mqtt_manager.cpp`.

## Project Structure

The project is organized into several components for clarity and separation of concerns.
