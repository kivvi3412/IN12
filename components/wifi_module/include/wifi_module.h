#ifndef ESP_TEST_WIFI_MODULE_H
#define ESP_TEST_WIFI_MODULE_H

#include "esp_wifi.h"
#include "esp_log.h"
#include <lwip/ip4_addr.h>
#include "esp_mac.h"
#include "mdns.h"

#include "nvs_module.h"

#define EXAMPLE_ESP_AP_WIFI_SSID       "IN12"
#define EXAMPLE_ESP_AP_WIFI_PASS       "12345678"
#define EXAMPLE_ESP_AP_WIFI_CHANNEL    1
#define EXAMPLE_MAX_STA_CONN           4


class WifiModule {
public:
    /**
     * @brief 初始化WiFi
     */
    static void init();

    /**
     * @brief 重新连接WiFi
     */
    static void staReconnect();
};

#endif // ESP_TEST_WIFI_MODULE_H