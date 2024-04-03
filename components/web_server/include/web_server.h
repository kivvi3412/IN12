//
// Created by HAIRONG ZHU on 2024/3/27.
//

#ifndef IN12_WEB_SERVER_H
#define IN12_WEB_SERVER_H

#include <cstring>
#include <lwip/ip4_addr.h>
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include <sys/param.h>
#include <string>
#include "cJSON.h"

#include "timezone_module.h"
#include "display_module.h"
#include "nvs_module.h"
#include "wifi_module.h"
#include "littlefs_module.h"

class WebServer {
public:
    static httpd_handle_t start();
};


#endif //IN12_WEB_SERVER_H
