//
// Created by HAIRONG ZHU on 2024/3/27.
//

#ifndef IN12_WEB_SERVER_H
#define IN12_WEB_SERVER_H

#include "esp_http_server.h"
#include <string>

#define LITTLEFS_MOUNT_POINT "/littlefs_root"

class WebServer {
public:
    /// 初始化HTTP服务: 注册所有API路由 + 静态文件兜底
    static httpd_handle_t init();

private:
    // ── API 路由注册（必须在通配符 /* 之前）──
    static void register_api_routes(httpd_handle_t server);

    // ── 管子/氖泡设置 ──
    static esp_err_t light_data_get_handler(httpd_req_t *req);
    static esp_err_t light_data_post_handler(httpd_req_t *req);

    // ── 时间设置 ──
    static esp_err_t time_data_get_handler(httpd_req_t *req);
    static esp_err_t time_data_post_handler(httpd_req_t *req);

    // ── WiFi ──
    static esp_err_t wifi_list_handler(httpd_req_t *req);
    static esp_err_t wifi_scan_handler(httpd_req_t *req);
    static esp_err_t wifi_connect_handler(httpd_req_t *req);
    static esp_err_t wifi_status_handler(httpd_req_t *req);

    // ── 恢复出厂设置 ──
    static esp_err_t reset_handler(httpd_req_t *req);

    // ── 工具: 读取 POST 请求体 ──
    static std::string read_post_body(httpd_req_t *req);
};

#endif //IN12_WEB_SERVER_H
