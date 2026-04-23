//
// Created by HAIRONG ZHU on 2024/3/27.
//

#ifndef IN12_NVS_MODULE_H
#define IN12_NVS_MODULE_H

#include "nvs_flash.h"
#include "esp_log.h"

class NVSModule {
public:
    static esp_err_t init_nvs();

    /**
     * 将任意 POD 结构体保存到 NVS。
     * @tparam T  必须是 trivially-copyable 的结构体（POD）
     * @param ns  NVS namespace（最长 15 字符）
     * @param key NVS key（最长 15 字符）
     * @param data 要保存的数据
     * @return ESP_OK 或错误码
     */
    template<typename T>
    static esp_err_t save(const char *ns, const char *key, const T &data);

    /**
     * 从 NVS 读取任意 POD 结构体。
     * 读取失败时 data 保持不变（保留调用方设置的默认值）。
     * 如果存储的 blob 大小与 sizeof(T) 不匹配（固件升级后结构体变化），
     * 返回 ESP_ERR_NVS_INVALID_LENGTH 并保留 data 默认值。
     * @tparam T  必须是 trivially-copyable 的结构体（POD）
     * @param ns  NVS namespace
     * @param key NVS key
     * @param data 读取目标，失败时保持不变
     * @return ESP_OK 或错误码
     */
    template<typename T>
    static esp_err_t load(const char *ns, const char *key, T &data);
};

template<typename T>
esp_err_t NVSModule::save(const char *ns, const char *key, const T &data) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVSModule", "save: nvs_open(%s) failed: %s", ns, esp_err_to_name(err));
        return err;
    }
    err = nvs_set_blob(handle, key, &data, sizeof(T));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    } else {
        ESP_LOGE("NVSModule", "save: nvs_set_blob(%s|%s) failed: %s", ns, key, esp_err_to_name(err));
    }
    nvs_close(handle);
    return err;
}

template<typename T>
esp_err_t NVSModule::load(const char *ns, const char *key, T &data) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        // ESP_ERR_NVS_NOT_FOUND 是正常情况（首次启动），不作为错误打印
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW("NVSModule", "load: nvs_open(%s) failed: %s", ns, esp_err_to_name(err));
        }
        return err;
    }
    size_t expected_size = sizeof(T);
    err = nvs_get_blob(handle, key, &data, &expected_size);
    if (err == ESP_OK && expected_size != sizeof(T)) {
        // 结构体大小变了（固件升级），安全地拒绝旧数据
        err = ESP_ERR_NVS_INVALID_LENGTH;
        ESP_LOGW("NVSModule", "load: size mismatch for %s|%s (got %d, expected %d), using defaults",
                 ns, key, (int)expected_size, (int)sizeof(T));
    }
    nvs_close(handle);
    return err;
}

#endif // IN12_NVS_MODULE_H
