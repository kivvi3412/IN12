//
// Created by HAIRONG ZHU on 2024/3/28.
//

#ifndef ESP_TEST_LITTLEFS_MODULE_H
#define ESP_TEST_LITTLEFS_MODULE_H
#include "esp_littlefs.h"
#include <esp_log.h>


class littleFS {
public:
    static void init();

    // std::string a = littleFS::readFile("index.html");
    static std::string readFile(const char* path);

    // char *a = (char *)malloc(4096);
    static void readFileToBuffer(const char* path, char* buffer, size_t size);
};


#endif //ESP_TEST_LITTLEFS_MODULE_H
