#ifndef IN12_WIFI_MODULE_H
#define IN12_WIFI_MODULE_H

#include "esp_wifi.h"
#include "esp_log.h"
#include <lwip/ip4_addr.h>
#include "esp_mac.h"
#include "mdns.h"
#include "cJSON.h"

#include "nvs_module.h"
#include "timezone_module.h"

#define EXAMPLE_ESP_STA_WIFI_SSID      "WIFI_SSID"
#define EXAMPLE_ESP_STA_WIFI_PASS      "PASSWORD"
#define EXAMPLE_ESP_AP_WIFI_SSID       "IN12"
#define EXAMPLE_ESP_AP_WIFI_PASS       "12345678"
#define EXAMPLE_ESP_AP_WIFI_CHANNEL    1
#define EXAMPLE_MAX_STA_CONN           4
#define DEFAULT_SCAN_LIST_SIZE         20


class WifiModule {
public:
    inline static bool is_sta_connected = false;
    inline static int disconnected_reason = 0;
    inline static cJSON *wifi_scan_init_list_result = cJSON_CreateArray();

    /**
     * @brief 初始化WiFi
     */
    static void init();

    /**
     * @brief 重新连接WiFi
     */
    static void staReconnect();

    /**
     * @brief 更新WiFi列表
     */
    static void renewWifiList();

    /**
     * @brief 获取WiFi列表
     * @param result
     */
    static void getAllInfoToJson(std::string &result);

    /*
     * @brief 打印STA断开原因
     */
    static const char *print_sta_disconnected_reason(int reason);
};

#endif // IN12_WIFI_MODULE_H