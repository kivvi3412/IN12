//
// Created by HAIRONG ZHU on 2026/4/20.
//

#include "wifi_module.h"
#include "nvs_module.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "mdns.h"
#include <ArduinoJson.h>
#include <cstring>
#include <lwip/ip4_addr.h>

static const char *TAG = "WifiModule";

// 静态成员变量必须在 cpp 中分配内存
EventGroupHandle_t WifiModule::wifi_event_group;
WifiState WifiModule::current_state = WifiState::IDLE;
WifiFailReason WifiModule::last_fail_reason = WifiFailReason::NONE;
std::string WifiModule::cached_wifi_list = "[]";
bool WifiModule::has_saved_wifi_config = false;
int WifiModule::raw_disconnect_reason = 0;
static SemaphoreHandle_t wifi_list_mutex = xSemaphoreCreateMutex();

void WifiModule::wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (has_saved_wifi_config) {
            current_state = WifiState::CONNECTING;
            esp_wifi_connect();
            ESP_LOGI(TAG, "已保存的 Wi-Fi 记录，尝试自动连接...");
        } else {
            current_state = WifiState::IDLE;
            ESP_LOGI(TAG, "没有保存的 Wi-Fi 记录，进入等待配网状态");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        auto *event = static_cast<ip_event_got_ip_t *>(event_data);
        ESP_LOGI(TAG, "已获取 IP 地址: " IPSTR, IP2STR(&event->ip_info.ip));

        current_state = WifiState::CONNECTED_LOCAL;
        last_fail_reason = WifiFailReason::NONE;

        // 发送成功事件，唤醒所有在等待连网的任务
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        auto *event = static_cast<wifi_event_sta_disconnected_t *>(event_data);
        raw_disconnect_reason = event->reason;

        // 状态机翻译：底层错误码 -> 前端人类可读错误
        switch (event->reason) {
            case WIFI_REASON_NO_AP_FOUND:
                last_fail_reason = WifiFailReason::AP_NOT_FOUND;
                break;
            case WIFI_REASON_AUTH_FAIL:
            case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
            case WIFI_REASON_MIC_FAILURE:
                last_fail_reason = WifiFailReason::WRONG_PASSWORD;
                break;
            case WIFI_REASON_BEACON_TIMEOUT:
                last_fail_reason = WifiFailReason::NETWORK_UNSTABLE;
                break;
            default:
                last_fail_reason = WifiFailReason::UNKNOWN_ERROR;
                break;
        }

        ESP_LOGW(TAG, "Wi-Fi 断开连接，原因代码: %d", event->reason);

        if (current_state == WifiState::SWITCHING_AP) { // 手动切换 AP 时，不自动重连
            current_state = WifiState::DISCONNECTED;
        } else if (event->reason == WIFI_REASON_AUTH_FAIL ||
                   event->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                   event->reason == WIFI_REASON_MIC_FAILURE) {
            // 密码错误，永久性失败，上报给配网任务
            current_state = WifiState::DISCONNECTED;
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        } else {
            // 其他一切（路由器重启、信号丢失、扫不到AP等）均为暂时性故障，持续重连
            current_state = WifiState::CONNECTING;
            ESP_LOGI(TAG, "尝试重新连接... (reason: %d)", event->reason);
            esp_wifi_connect();
        }
    }
}

void WifiModule::init() {
    // 1. 创建事件组
    wifi_event_group = xEventGroupCreate();

    // 2. 基础网络初始化
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();

    // 配置 AP 的固定 IP (比如 192.168.10.1)
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 10, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 10, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(ap_netif);
    esp_netif_set_ip_info(ap_netif, &ip_info);
    esp_netif_dhcps_start(ap_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 3. 注册事件处理
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr, nullptr));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr, nullptr));

    // 4. 配置并启动 (AP + STA 模式)
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    // -- AP 配置 (固定不变的配网热点) --
    wifi_config_t ap_config = {};
    ap_config.ap.ssid_len = strlen(WIFI_AP_SSID);
    ap_config.ap.channel = WIFI_AP_CHANNEL;
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap_config.ap.max_connection = WIFI_AP_MAX_CONN;
    strncpy(reinterpret_cast<char *>(ap_config.ap.ssid), WIFI_AP_SSID, sizeof(ap_config.ap.ssid));
    strncpy(reinterpret_cast<char *>(ap_config.ap.password), WIFI_AP_PASS, sizeof(ap_config.ap.password));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    // -- STA 配置 (从 NVS 读取) --
    wifi_config_t sta_config = {};
    if (NVSModule::load(NVS_NS_WIFI, NVS_KEY_WIFI, sta_config) == ESP_OK) {
        ESP_LOGI(TAG, "发现已保存的 Wi-Fi 记录: %s", sta_config.sta.ssid);
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
        has_saved_wifi_config = true;
    } else {
        ESP_LOGW(TAG, "NVS 中无 Wi-Fi 记录，进入等待配网状态");
        has_saved_wifi_config = false;
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    init_mdns();
    // refresh_wifi_list(); // 开机扫描电流过大可能会导致缺电重启
}

void WifiModule::init_mdns() {
    mdns_init();
    mdns_hostname_set(MDNS_HOSTNAME);
    mdns_instance_name_set(MDNS_INSTANCE_NAME);
}

bool WifiModule::connect_to_new_ap(const std::string &ssid, const std::string &password) {
    ESP_LOGI(TAG, "接收到前端配网请求，尝试连接: %s", ssid.c_str());

    // 切换到专用状态，阻止事件处理函数中的自动重连
    current_state = WifiState::SWITCHING_AP;

    // 断开当前连接
    esp_wifi_disconnect();

    // 等一下让断开事件处理完毕，避免 STA 状态机还没切换就改配置
    vTaskDelay(pdMS_TO_TICKS(200));

    // 准备新的配置参数
    wifi_config_t wifi_config = {};
    strncpy(reinterpret_cast<char *>(wifi_config.sta.ssid), ssid.c_str(), sizeof(wifi_config.sta.ssid));
    strncpy(reinterpret_cast<char *>(wifi_config.sta.password), password.c_str(), sizeof(wifi_config.sta.password));

    // 应用新配置
    if (esp_wifi_set_config(WIFI_IF_STA, &wifi_config) != ESP_OK) {
        ESP_LOGE(TAG, "设置 WiFi 配置失败，放弃本次配网");
        current_state = WifiState::DISCONNECTED;
        return false;
    }

    current_state = WifiState::CONNECTING;

    // 关键点：清空标志位，防止上次的残留事件干扰
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    esp_wifi_connect();

    // 阻塞等待连接结果 (最多等 10 秒)
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(10000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "配网成功！保存到 NVS...");
        NVSModule::save(NVS_NS_WIFI, NVS_KEY_WIFI, wifi_config);
        return true;
    }

    ESP_LOGE(TAG, "配网失败！将不会保存此配置");
    return false;
}

std::string WifiModule::get_status_json() {
    JsonDocument doc;

    // 将 Enum 转换为前端能懂的字符串
    switch (current_state) {
        case WifiState::CONNECTING: doc["state"] = "CONNECTING";
            break;
        case WifiState::CONNECTED_LOCAL: doc["state"] = "CONNECTED";
            break;
        case WifiState::DISCONNECTED: doc["state"] = "DISCONNECTED";
            break;
        default: doc["state"] = "IDLE";
            break;
    }

    switch (last_fail_reason) {
        case WifiFailReason::WRONG_PASSWORD: doc["error"] = "WRONG_PASSWORD";
            break;
        case WifiFailReason::AP_NOT_FOUND: doc["error"] = "AP_NOT_FOUND";
            break;
        case WifiFailReason::NONE: doc["error"] = "NONE";
            break;
        default: doc["error"] = "UNKNOWN";
            break;
    }

    // 从底层取一下当前连接的 SSID 名字反馈给前端
    wifi_config_t conf;
    esp_wifi_get_config(WIFI_IF_STA, &conf);
    doc["current_ssid"] = reinterpret_cast<char *>(conf.sta.ssid);

    std::string output;
    serializeJson(doc, output);
    return output;
}

void WifiModule::refresh_wifi_list() {
    WifiState previous_state = current_state;
    current_state = WifiState::SCANNING;

    uint16_t number = WIFI_SCAN_MAX;
    wifi_ap_record_t ap_info[WIFI_SCAN_MAX];

    esp_wifi_scan_start(nullptr, true); // true 表示阻塞等待扫描完成
    esp_wifi_scan_get_ap_records(&number, ap_info);

    JsonDocument doc;
    doc.to<JsonArray>();

    for (int i = 0; i < number; i++) {
        auto item = doc.add<JsonObject>();
        item["ssid"] = reinterpret_cast<const char *>(ap_info[i].ssid);
        item["rssi"] = ap_info[i].rssi;
        item["auth"] = ap_info[i].authmode;
    }

    // 更新到全局缓存字符串中
    std::string temp_str;
    serializeJson(doc, temp_str);

    xSemaphoreTake(wifi_list_mutex, portMAX_DELAY);
    cached_wifi_list = std::move(temp_str);
    xSemaphoreGive(wifi_list_mutex);

    // 扫描完，恢复以前的状态 (连着网的还是连着网)
    current_state = previous_state;
}

std::string WifiModule::get_wifi_list() {
    xSemaphoreTake(wifi_list_mutex, portMAX_DELAY);
    std::string response = cached_wifi_list;
    xSemaphoreGive(wifi_list_mutex);
    return response;
}
