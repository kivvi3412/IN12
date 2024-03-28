//
// Created by HAIRONG ZHU on 2024/3/27.
//

#ifndef ESP_TEST_NVS_MODULE_H
#define ESP_TEST_NVS_MODULE_H

#include "nvs_flash.h"
#include "nvs.h"

#include "esp_wifi.h"

class NVSModule {
public:
    static esp_err_t init_nvs();

    static esp_err_t save_wifi_config(const wifi_config_t *wifi_config);

    static esp_err_t load_wifi_config(wifi_config_t *wifi_config);
};

#endif //ESP_TEST_NVS_MODULE_H
