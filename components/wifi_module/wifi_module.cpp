//
// Created by HAIRONG ZHU on 2024/3/27.
//

#include "wifi_module.h"


static const char *TAG = "wifi_module";

bool WifiModule::is_sta_connected = false;


void WifiModule::set_wifi_status(bool status) {
    is_sta_connected = status;
}

bool WifiModule::wifi_is_connected() {
    return is_sta_connected;
}


void wifiEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {   // 有设备连接
        auto event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) { // 有设备断开
        auto *event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {  // STA 开始
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) { // STA 获取IP
        auto *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        WifiModule::set_wifi_status(true);
        TimezoneModule::set_system_timezone();
        ESP_LOGI(TAG, "set timezone success");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) { // STA 断开
        ESP_LOGI(TAG, "Wi-Fi disconnected");
        WifiModule::set_wifi_status(false);
    }
}

static void wifiInitAp() {
    wifi_config_t wifi_config = {
            .ap = {
                    .ssid = EXAMPLE_ESP_AP_WIFI_SSID,
                    .password = EXAMPLE_ESP_AP_WIFI_PASS,
                    .ssid_len = static_cast<uint8_t>(strlen(EXAMPLE_ESP_AP_WIFI_SSID)),
                    .channel = EXAMPLE_ESP_AP_WIFI_CHANNEL,
                    .authmode = WIFI_AUTH_WPA2_PSK,
                    .max_connection = EXAMPLE_MAX_STA_CONN,
            },
    };


    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 10, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 10, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(netif));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(netif));
}

static void wifiInitSta() {
    // 从NVS中加载WIFI配置
    wifi_config_t wifi_config;
    esp_err_t err = NVSModule::load_wifi_config(&wifi_config);
    if (err != ESP_OK) {    // 加载失败
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "WiFi configuration not found in NVS");
            // 设置默认的WIFI配置
            wifi_config = {
                    .sta = {
                            .ssid = EXAMPLE_ESP_STA_WIFI_SSID,
                            .password = EXAMPLE_ESP_STA_WIFI_PASS,
                    },
            };
        } else {
            ESP_LOGE(TAG, "Failed to load WiFi configuration from NVS");
        }
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 获取WIFI账号密码
    ESP_LOGI(TAG, "Connecting to %s, Password: %s", wifi_config.sta.ssid, wifi_config.sta.password);

    esp_wifi_connect();
}

// 内网做域名解析
static void initMdns() {
    mdns_init();
    mdns_hostname_set("IN12");
    mdns_instance_name_set("IN12 mDNS");

    mdns_txt_item_t serviceTxtData[] = {
            {"board", "esp32"},
            {"path",  "/"}
    };

    ESP_ERROR_CHECK(mdns_service_add("IN12", "_http", "_tcp", 80, serviceTxtData,
                                     sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));
}

void WifiModule::init() {
    ESP_ERROR_CHECK(esp_netif_init());  // 初始化网络接口
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // WIFI事件循环

    esp_netif_create_default_wifi_ap(); // 创建默认的WIFI AP
    esp_netif_create_default_wifi_sta(); // 创建默认的WIFI STA

    // WIFI 驱动初始化
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册事件处理程序
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifiEventHandler,
                                                        nullptr,
                                                        nullptr));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifiEventHandler,
                                                        nullptr,
                                                        nullptr));

    // 初始化热点模式
    wifiInitAp();
    // 初始化STA模式
    wifiInitSta();
    // 初始化mDNS
    initMdns();
}

// 重新连接WIFI
void WifiModule::staReconnect() {
    ESP_ERROR_CHECK(esp_wifi_disconnect());

    // 从NVS中加载Wi-Fi配置
    wifi_config_t wifi_config;
    ESP_ERROR_CHECK(NVSModule::load_wifi_config(&wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    esp_wifi_connect();
}


