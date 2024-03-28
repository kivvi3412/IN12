//
// Created by HAIRONG ZHU on 2024/3/27.
//

#ifndef ESP_TEST_WIFI_MODULE_H
#define ESP_TEST_WIFI_MODULE_H

#include "esp_wifi.h"
#include "esp_log.h"
#include <lwip/ip4_addr.h>
#include "esp_mac.h"
#include "mdns.h"

#include "nvs_module.h"

#define EXAMPLE_ESP_STA_WIFI_SSID      "AirPort-5G"
#define EXAMPLE_ESP_STA_WIFI_PASS      "###732###"
#define EXAMPLE_ESP_AP_WIFI_SSID       "IN12"
#define EXAMPLE_ESP_AP_WIFI_PASS       "12345678"
#define EXAMPLE_ESP_AP_WIFI_CHANNEL    1
#define EXAMPLE_MAX_STA_CONN           4

void wifi_init();

void wifi_reconnect();

#endif //ESP_TEST_WIFI_MODULE_H
