idf_component_register(SRCS "main.cpp" "src/WifiManager.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp_wifi esp_event esp_timer log console)