//
// Created by HAIRONG ZHU on 2024/3/28.
//

#include <string>
#include <cstring>
#include "littlefs_module.h"

#define LITTLEFS_PARTITION_LABEL "littlefs"
#define LITTLEFS_MOUNT_POINT "/littlefs_root"

static const char *TAG = "LittleFS";

void littleFS::init() {
    esp_vfs_littlefs_conf_t conf = {
            .base_path = LITTLEFS_MOUNT_POINT,
            .partition_label = LITTLEFS_PARTITION_LABEL,
            .format_if_mount_failed = true,
            .dont_mount = false,
    };

    // 使用给定的配置初始化和挂载LittleFS。
    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return;
    }
    ESP_ERROR_CHECK(ret);

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    ESP_ERROR_CHECK(ret);
}

std::string littleFS::readFile(const char *relativePath) {
    std::string absolutePath = LITTLEFS_MOUNT_POINT + std::string("/") + std::string(relativePath);
    FILE *file = fopen(absolutePath.c_str(), "r");
    if (file == nullptr) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return {};
    }

    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    std::string content;
    content.resize(fileSize);

    size_t bytesRead = fread(&content[0], 1, fileSize, file);
    if (bytesRead != fileSize) {
        ESP_LOGE(TAG, "Failed to read file");
        fclose(file);
        return {};
    }

    fclose(file);
    return content;
}

void littleFS::readFileToBuffer(const char *relativePath, char *buffer, size_t buffer_size) {
    std::string absolutePath = LITTLEFS_MOUNT_POINT + std::string("/") + std::string(relativePath);
    FILE *file = fopen(absolutePath.c_str(), "r");
    if (file == nullptr) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        memset(buffer, 0, buffer_size);  // 如果文件无法打开，清空缓冲区
        return;
    }

    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    if (fileSize > buffer_size - 1) {  // 确保缓冲区能够容纳文件内容和一个空字符
        ESP_LOGE(TAG, "Buffer size is too small");
        fclose(file);
        memset(buffer, 0, buffer_size);  // 如果缓冲区大小不足，清空缓冲区
        return;
    }

    size_t bytesRead = fread(buffer, 1, fileSize, file);
    if (bytesRead != fileSize) {
        ESP_LOGE(TAG, "Failed to read file");
        fclose(file);
        memset(buffer, 0, buffer_size);  // 如果读取失败，清空缓冲区
        return;
    }

    buffer[bytesRead] = '\0';  // 在文件内容的末尾添加空字符

    fclose(file);
}