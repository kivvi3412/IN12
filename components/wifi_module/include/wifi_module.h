#ifndef IN12_WIFI_MODULE_H
#define IN12_WIFI_MODULE_H

#include "esp_wifi.h"
#include "esp_log.h"
#include <lwip/ip4_addr.h>
#include "esp_mac.h"
#include "mdns.h"

#include "nvs_module.h"
#include "timezone_module.h"

#define EXAMPLE_ESP_AP_WIFI_SSID       "IN12"
#define EXAMPLE_ESP_AP_WIFI_PASS       "12345678"
#define EXAMPLE_ESP_AP_WIFI_CHANNEL    1
#define EXAMPLE_MAX_STA_CONN           4


class WifiModule {
public:
    static bool is_sta_connected;

    /**
     * @brief 初始化WiFi
     */
    static void init();

    /**
     * @brief 重新连接WiFi
     */
    static void staReconnect();

    /**
     * @brief 设置WiFi状态
     * @param status
     */
    static void set_wifi_status(bool status);

    /**
     * @brief 获取WiFi状态
     * @return
     */
    static bool wifi_is_connected();
};

#endif // IN12_WIFI_MODULE_H