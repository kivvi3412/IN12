idf_component_register(SRCS "controller.cpp"
        INCLUDE_DIRS "include"
        REQUIRES in12_driver timezone_module nvs_module nvs_flash esp_timer esp_system project_config
)
