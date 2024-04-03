//
// Created by HAIRONG ZHU on 2024/3/27.
//

#include "wifi_module.h"


static const char *WIFI_TAG = "wifiModule";

const char *WifiModule::print_sta_disconnected_reason(int reason) {
    switch (reason) {
        case WIFI_REASON_UNSPECIFIED:
            return "WIFI_REASON_UNSPECIFIED: Unspecified reason";
        case WIFI_REASON_AUTH_EXPIRE:
            return "WIFI_REASON_AUTH_EXPIRE: Authentication expired";
        case WIFI_REASON_AUTH_LEAVE:
            return "WIFI_REASON_AUTH_LEAVE: Authentication left";
        case WIFI_REASON_ASSOC_EXPIRE:
            return "WIFI_REASON_ASSOC_EXPIRE: Association expired";
        case WIFI_REASON_ASSOC_TOOMANY:
            return "WIFI_REASON_ASSOC_TOOMANY: Too many associations";
        case WIFI_REASON_NOT_AUTHED:
            return "WIFI_REASON_NOT_AUTHED: Not authenticated";
        case WIFI_REASON_NOT_ASSOCED:
            return "WIFI_REASON_NOT_ASSOCED: Not associated";
        case WIFI_REASON_ASSOC_LEAVE:
            return "WIFI_REASON_ASSOC_LEAVE: Association left";
        case WIFI_REASON_ASSOC_NOT_AUTHED:
            return "WIFI_REASON_ASSOC_NOT_AUTHED: Association not authenticated";
        case WIFI_REASON_BSS_TRANSITION_DISASSOC:
            return "WIFI_REASON_BSS_TRANSITION_DISASSOC: BSS transition disassociation";
        case WIFI_REASON_IE_INVALID:
            return "WIFI_REASON_IE_INVALID: Invalid IE";
        case WIFI_REASON_MIC_FAILURE:
            return "WIFI_REASON_MIC_FAILURE: MIC failure";
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
            return "WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: Password incorrect";
        case WIFI_REASON_BEACON_TIMEOUT:
            return "WIFI_REASON_BEACON_TIMEOUT: Beacon timeout";
        case WIFI_REASON_NO_AP_FOUND:
            return "WIFI_REASON_NO_AP_FOUND: No AP found";
        case WIFI_REASON_AUTH_FAIL:
            return "WIFI_REASON_AUTH_FAIL: Authentication failed";
        case WIFI_REASON_ASSOC_FAIL:
            return "WIFI_REASON_ASSOC_FAIL: Association failed";
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            return "WIFI_REASON_HANDSHAKE_TIMEOUT: Handshake timeout";
        case WIFI_REASON_CONNECTION_FAIL:
            return "WIFI_REASON_CONNECTION_FAIL: Connection failed";
        default:
            return "Connecting...";
    }
}

void wifiEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {   // 有设备连接
        auto event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(WIFI_TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) { // 有设备断开
        auto *event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(WIFI_TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {  // STA 开始
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) { // STA 获取IP
        auto *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        WifiModule::is_sta_connected = true;
        TimezoneModule::set_system_timezone();
        ESP_LOGI(WIFI_TAG, "set timezone success");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) { // STA 断开
        // 打印断开原因
        auto *event = (wifi_event_sta_disconnected_t *) event_data;
        ESP_LOGI(WIFI_TAG, "Disconnect reason : %s", WifiModule::print_sta_disconnected_reason(event->reason));
        WifiModule::disconnected_reason = event->reason;
        WifiModule::is_sta_connected = false;
    }
}

static void wifiScan(cJSON *root) {    // 获取WIFI列表
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    esp_wifi_scan_start(nullptr, true);
    ESP_LOGI(WIFI_TAG, "Max AP number ap_info can hold = %u", number);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(WIFI_TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);

    for (int i = 0; i < number; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", (const char *) ap_info[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", ap_info[i].rssi);
        cJSON_AddNumberToObject(item, "authmode", ap_info[i].authmode);
        cJSON_AddNumberToObject(item, "channel", ap_info[i].primary);   // 信道
        cJSON_AddItemToArray(root, item);
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
            ESP_LOGI(WIFI_TAG, "WiFi configuration not found in NVS");
            // 设置默认的WIFI配置
            wifi_config = {
                    .sta = {
                            .ssid = EXAMPLE_ESP_STA_WIFI_SSID,
                            .password = EXAMPLE_ESP_STA_WIFI_PASS,
                    },
            };
        } else {
            ESP_LOGE(WIFI_TAG, "Failed to load WiFi configuration from NVS");
        }
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 获取WIFI账号密码
    ESP_LOGI(WIFI_TAG, "Connecting to %s, Password: %s", wifi_config.sta.ssid, wifi_config.sta.password);
    esp_wifi_connect();
}

static void initMdns() {    // 内网做域名解析
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

void WifiModule::staReconnect() {   // 重新连接WIFI
    ESP_ERROR_CHECK(esp_wifi_disconnect());

    // 从NVS中加载Wi-Fi配置
    wifi_config_t wifi_config;
    ESP_ERROR_CHECK(NVSModule::load_wifi_config(&wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    esp_wifi_connect();
}

void WifiModule::renewWifiList() {  // 更新WIFI列表
    // 清空原有的列表
    cJSON_Delete(wifi_scan_init_list_result);
    wifi_scan_init_list_result = cJSON_CreateArray();
    // 重新获取WIFI列表
    wifiScan(wifi_scan_init_list_result);
}

void WifiModule::getAllInfoToJson(std::string &result) {
    cJSON *root = cJSON_CreateObject();

    // 获取WIFI账号密码
    wifi_config_t wifi_config;
    ESP_ERROR_CHECK(NVSModule::load_wifi_config(&wifi_config));
    cJSON *wifi_config_json = cJSON_CreateObject();

    // 添加WIFI连接状态
    cJSON_AddNumberToObject(wifi_config_json, "connected", WifiModule::is_sta_connected);
    cJSON_AddStringToObject(wifi_config_json, "disconnected_reason",
                            print_sta_disconnected_reason(WifiModule::disconnected_reason));

    // 添加WIFI账号密码
    cJSON_AddStringToObject(wifi_config_json, "ssid", (const char *) wifi_config.sta.ssid);
    cJSON_AddStringToObject(wifi_config_json, "password", (const char *) wifi_config.sta.password);
    cJSON_AddItemToObject(root, "wifi_config", wifi_config_json);

    // 获取WIFI列表，进行深拷贝，否则会被一起释放
    cJSON *wifi_list_copy = cJSON_Duplicate(WifiModule::wifi_scan_init_list_result, true);
    cJSON_AddItemToObject(root, "wifi_list", wifi_list_copy);

    // 将JSON对象转换为字符串
    char *json_str = cJSON_Print(root);
    if (json_str != nullptr) {
        result = json_str;
        cJSON_free(json_str);  // 释放cJSON_Print()分配的内存
    }

    cJSON_Delete(root);  // 删除root对象，它会自动删除所有的子项
}



