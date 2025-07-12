idf_component_register(SRCS "main.cpp"
                           "wifi_manager.cpp"
                           "time_sync.cpp"
                           "mqtt_manager.cpp"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES "certs/ca.crt"
                                 "certs/client.crt"
                                 "certs/client.key")