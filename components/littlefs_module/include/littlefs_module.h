//
// Created by HAIRONG ZHU on 2024/3/28.
//

#ifndef IN12_LITTLEFS_MODULE_H
#define IN12_LITTLEFS_MODULE_H
#include "esp_littlefs.h"
#include <esp_log.h>
#include <string>
#include <cstring>


class littleFS {
public:
    static void init();

    // std::string a = littleFS::readFile("index.html");
    static std::string readFile(const char* path);

    // char *a = (char *)malloc(4096);
    static void readFileToBuffer(const char* path, char* buffer, size_t size);
};


#endif //IN12_LITTLEFS_MODULE_H
