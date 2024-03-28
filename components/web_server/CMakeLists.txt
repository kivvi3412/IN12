idf_component_register(SRCS "web_server.cpp"
        INCLUDE_DIRS "include"
        REQUIRES esp_http_server nvs_module json wifi_module littlefs_module
        )
