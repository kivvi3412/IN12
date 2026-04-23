//
// Created by HAIRONG ZHU on 2026/4/20.
//

#ifndef IN12_WIFI_MODULE_H
#define IN12_WIFI_MODULE_H

#include <string>
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "project_config.h"   // WIFI_AP_SSID / PASS / CHANNEL / MAX_CONN, WIFI_SCAN_MAX

// 定义设备当前的网络阶段
enum class WifiState {
    IDLE,
    SCANNING,
    CONNECTING,
    SWITCHING_AP,    // 正在主动切换到新 AP（阻止事件处理器自动重连）
    CONNECTED_LOCAL, // 已连接路由器
    DISCONNECTED
};

// 定义给前端展示的错误原因
enum class WifiFailReason {
    NONE,
    WRONG_PASSWORD,    // 密码错误
    AP_NOT_FOUND,      // 找不到热点
    NETWORK_UNSTABLE,  // 网络不稳定掉线
    UNKNOWN_ERROR      // 其他未知错误
};

class WifiModule {
public:
    // 初始化网络、NVS 读取、启动 AP 和 STA
    static void init();

    // 扫描附近的 WiFi 热点
    static void refresh_wifi_list();

    // 获取当前的 WiFi 列表
    static std::string get_wifi_list();

    // 供 Web 前端调用：配网尝试连接新的 AP
    static bool connect_to_new_ap(const std::string &ssid, const std::string &password);

    // 供 Web 前端调用：获取当前网络状态 (返回 JSON)
    static std::string get_status_json();

    // FreeRTOS 事件组句柄
    static EventGroupHandle_t wifi_event_group;
    static constexpr int WIFI_CONNECTED_BIT = BIT0;
    static constexpr int WIFI_FAIL_BIT      = BIT1;

private:
    // 全局状态维护
    static WifiState     current_state;
    static WifiFailReason last_fail_reason;
    static int           raw_disconnect_reason;
    static std::string   cached_wifi_list; // 用于缓存扫描结果
    static bool          has_saved_wifi_config;

    // ESP-IDF 底层事件回调函数
    static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data);

    // 初始化 MDNS
    static void init_mdns();
};

#endif // IN12_WIFI_MODULE_H
