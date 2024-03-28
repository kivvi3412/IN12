//
// Created by HAIRONG ZHU on 2024/3/27.
//

#include "nvs_module.h"

// 初始化NVS, 用于保存WiFi配置
esp_err_t NVSModule::init_nvs() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

// 用于保存WIFI结构体
esp_err_t NVSModule::save_wifi_config(const wifi_config_t *wifi_config) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_config", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(nvs_handle, "wifi_config", wifi_config, sizeof(*wifi_config));
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err;
}

// 用于加载WIFI结构体
esp_err_t NVSModule::load_wifi_config(wifi_config_t *wifi_config) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_config", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    size_t required_size = sizeof(*wifi_config); // 获取期望的大小
    err = nvs_get_blob(nvs_handle, "wifi_config", wifi_config, &required_size);
    nvs_close(nvs_handle);
    return err;
}