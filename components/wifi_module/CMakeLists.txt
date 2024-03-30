idf_component_register(SRCS "wifi_module.cpp"
        INCLUDE_DIRS "include"
        REQUIRES esp_wifi nvs_flash mdns nvs_module timezone_module
        )
