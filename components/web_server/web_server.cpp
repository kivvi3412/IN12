#include "web_server.h"
#include "controller.h"
#include "wifi_module.h"
#include "timezone_module.h"

#include <string>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <esp_log.h>
#include <ArduinoJson.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "WebServer";

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  静态文件服务
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
static void set_content_type_from_file(httpd_req_t *req, const std::string &filepath) {
    if (filepath.find(".html") != std::string::npos) {
        httpd_resp_set_type(req, "text/html");
    } else if (filepath.find(".css") != std::string::npos) {
        httpd_resp_set_type(req, "text/css");
    } else if (filepath.find(".js") != std::string::npos) {
        httpd_resp_set_type(req, "application/javascript");
    } else if (filepath.find(".png") != std::string::npos) {
        httpd_resp_set_type(req, "image/png");
    } else if (filepath.find(".jpg") != std::string::npos || filepath.find(".jpeg") != std::string::npos) {
        httpd_resp_set_type(req, "image/jpeg");
    } else if (filepath.find(".ico") != std::string::npos) {
        httpd_resp_set_type(req, "image/x-icon");
    } else if (filepath.find(".json") != std::string::npos) {
        httpd_resp_set_type(req, "application/json");
    } else if (filepath.find(".mp4") != std::string::npos) {
        httpd_resp_set_type(req, "video/mp4");
    }
    // 如果都不匹配，ESP-IDF 会默认使用 text/html 或由浏览器自行推断
}

// 处理静态文件请求的回调函数（支持 Range 请求，移动端视频流必需）
static esp_err_t file_get_handler(httpd_req_t *req) {
    std::string uri(req->uri);

    // 如果用户只输入了域名或IP (URI为 "/")，则定向到 index.html
    if (uri == "/") uri = "/index.html";
    std::string filepath = std::string(LITTLEFS_MOUNT_POINT) + uri;

    FILE *fd = fopen(filepath.c_str(), "rb"); // 二进制模式，图片/视频必须
    if (fd == nullptr) {
        ESP_LOGW(TAG, "File not found: %s", filepath.c_str());
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "404 Not Found - The requested file does not exist.");
        return ESP_FAIL;
    }

    // 获取文件总大小
    fseek(fd, 0, SEEK_END);
    long file_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    set_content_type_from_file(req, filepath);

    // 声明支持 Range 请求（移动端浏览器看到这个才会发 Range 头）
    httpd_resp_set_hdr(req, "Accept-Ranges", "bytes");

    // 解析 Range 请求头（移动端视频：Range: bytes=start-end 或 bytes=start-）
    char range_buf[64] = {};
    long range_start = 0;
    long range_end   = file_size - 1;
    bool is_range    = (httpd_req_get_hdr_value_str(req, "Range", range_buf, sizeof(range_buf)) == ESP_OK);

    if (is_range) {
        long s = 0, e = file_size - 1;
        int parsed = sscanf(range_buf, "bytes=%ld-%ld", &s, &e);
        if (parsed >= 1) {
            range_start = s;
            range_end   = (parsed == 2 && e < file_size) ? e : file_size - 1;
        }
        ESP_LOGI(TAG, "Range: %ld-%ld / %ld", range_start, range_end, file_size);
    }

    long content_length = range_end - range_start + 1;

    // Range 请求返回 206，否则正常 200
    if (is_range) {
        httpd_resp_set_status(req, "206 Partial Content");
        char cr_header[64];
        snprintf(cr_header, sizeof(cr_header), "bytes %ld-%ld/%ld", range_start, range_end, file_size);
        httpd_resp_set_hdr(req, "Content-Range", cr_header);
    }

    // 显式告知内容长度，让浏览器知道进度条总长
    char cl_header[24];
    snprintf(cl_header, sizeof(cl_header), "%ld", content_length);
    httpd_resp_set_hdr(req, "Content-Length", cl_header);

    // 定位到请求的起始字节
    fseek(fd, range_start, SEEK_SET);

    // 分块发送（4KB 块，视频流效率更高）
    char chunk[4096];
    long remaining = content_length;
    while (remaining > 0) {
        size_t to_read = (remaining < (long)sizeof(chunk)) ? (size_t)remaining : sizeof(chunk);
        size_t nread   = fread(chunk, 1, to_read, fd);
        if (nread == 0) break;
        if (httpd_resp_send_chunk(req, chunk, (ssize_t)nread) != ESP_OK) {
            ESP_LOGE(TAG, "File sending failed!");
            fclose(fd);
            return ESP_FAIL;
        }
        remaining -= (long)nread;
    }

    fclose(fd);
    httpd_resp_send_chunk(req, nullptr, 0); // 结束 Chunked 传输
    return ESP_OK;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  POST Body 读取工具
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
std::string WebServer::read_post_body(httpd_req_t *req) {
    int total_len = req->content_len;
    if (total_len <= 0 || total_len > 2048) return "";

    std::string body(total_len, '\0');
    int received = 0;
    while (received < total_len) {
        int ret = httpd_req_recv(req, &body[received], total_len - received);
        if (ret <= 0) return "";
        received += ret;
    }
    body.resize(received);
    return body;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  /api/light_data — 管子/氖泡/展示设置
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
esp_err_t WebServer::light_data_get_handler(httpd_req_t *req) {
    const auto &s = Controller::getInstance().getSettings();

    JsonDocument doc;
    doc["tube_on"] = s.tube_on;
    doc["neon_on"] = s.neon_on;
    doc["neon_blink"] = s.neon_blink;

    doc["sleep_enabled"] = s.sleep_enabled;
    char time_buf[16];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", s.sleep_start_h, s.sleep_start_m);
    doc["sleep_start"] = time_buf;
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", s.sleep_end_h, s.sleep_end_m);
    doc["sleep_end"] = time_buf;

    doc["fixed_poison_enabled"] = s.fixed_poison_enabled;
    doc["fixed_poison_hour"] = s.fixed_poison_hour;
    doc["fixed_poison_duration"] = s.fixed_poison_duration;

    doc["hourly_poison_enabled"] = s.hourly_poison_enabled;

    doc["display_mode"] = s.display_mode;
    auto arr = doc["display_digits"].to<JsonArray>();
    (void) arr.add(s.display_d1);
    (void) arr.add(s.display_d2);
    (void) arr.add(s.display_d3);
    (void) arr.add(s.display_d4);

    std::string output;
    serializeJson(doc, output);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, output.c_str());
    return ESP_OK;
}

esp_err_t WebServer::light_data_post_handler(httpd_req_t *req) {
    std::string body = read_post_body(req);
    if (body.empty()) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }

    JsonDocument doc;
    if (deserializeJson(doc, body)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // 基于当前设置做部分合并更新（前端只需发送变更字段）
    LightSettings s = Controller::getInstance().getSettings();

    if (!doc["tube_on"].isNull()) s.tube_on = doc["tube_on"];
    if (!doc["neon_on"].isNull()) s.neon_on = doc["neon_on"];
    if (!doc["neon_blink"].isNull()) s.neon_blink = doc["neon_blink"];

    if (!doc["sleep_enabled"].isNull()) s.sleep_enabled = doc["sleep_enabled"];
    if (!doc["sleep_start"].isNull()) {
        int h, m;
        if (sscanf(doc["sleep_start"].as<const char *>(), "%d:%d", &h, &m) == 2) {
            s.sleep_start_h = h;
            s.sleep_start_m = m;
        }
    }
    if (!doc["sleep_end"].isNull()) {
        int h, m;
        if (sscanf(doc["sleep_end"].as<const char *>(), "%d:%d", &h, &m) == 2) {
            s.sleep_end_h = h;
            s.sleep_end_m = m;
        }
    }

    if (!doc["fixed_poison_enabled"].isNull()) s.fixed_poison_enabled = doc["fixed_poison_enabled"];
    if (!doc["fixed_poison_hour"].isNull()) s.fixed_poison_hour = doc["fixed_poison_hour"];
    if (!doc["fixed_poison_duration"].isNull()) s.fixed_poison_duration = doc["fixed_poison_duration"];
    if (!doc["hourly_poison_enabled"].isNull()) s.hourly_poison_enabled = doc["hourly_poison_enabled"];

    if (!doc["display_mode"].isNull()) s.display_mode = doc["display_mode"];
    if (!doc["display_digits"].isNull()) {
        auto arr = doc["display_digits"].as<JsonArray>();
        if (arr.size() >= 4) {
            s.display_d1 = arr[0];
            s.display_d2 = arr[1];
            s.display_d3 = arr[2];
            s.display_d4 = arr[3];
        }
    }

    Controller::getInstance().applySettings(s);

    // 返回更新后的完整设置（复用 GET handler）
    return light_data_get_handler(req);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  /api/time_data — 时间源/时区/NTP/手动校准
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
esp_err_t WebServer::time_data_get_handler(httpd_req_t *req) {
    auto &tm = TimeManager::getInstance();

    JsonDocument doc;
    doc["ntp_enabled"] = esp_sntp_enabled();

    const char *tz = getenv("TZ");
    doc["timezone"] = tz ? tz : "CST-8";
    doc["ntp_synced"] = tm.isNtpSynced();
    doc["last_sync"] = tm.getLastSyncTimeStr();

    // 当前系统时间
    time_t now;
    time(&now);
    struct tm timeinfo{};
    localtime_r(&now, &timeinfo);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    doc["current_time"] = buf;

    std::string output;
    serializeJson(doc, output);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, output.c_str());
    return ESP_OK;
}

esp_err_t WebServer::time_data_post_handler(httpd_req_t *req) {
    std::string body = read_post_body(req);
    if (body.empty()) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }

    JsonDocument doc;
    if (deserializeJson(doc, body)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    auto &tm = TimeManager::getInstance();

    // 切换 NTP 模式 (开关一切换就变成NTP)
    if (!doc["ntp_enabled"].isNull()) {
        tm.setNtpMode(doc["ntp_enabled"].as<bool>());
    }

    // 设置时区 (不持久化，重启回 CST-8)
    if (!doc["timezone"].isNull()) {
        const char *tz = doc["timezone"];
        setenv("TZ", tz, 1);
        tzset();
        Controller::getInstance().forceTimeRefresh();
    }

    // 手动设置时间 → 自动关闭 NTP，直接写 RTC，时钟继续走
    if (!doc["manual_time"].isNull()) {
        auto mt = doc["manual_time"];
        tm.setNtpMode(false); // 手动写时间自动关 NTP
        TimeManager::setManualTime(
            mt["year"], mt["month"], mt["day"],
            mt["hour"], mt["minute"], mt["second"]
        );
        Controller::getInstance().forceTimeRefresh();
    }

    // 强制 NTP 同步
    if (!doc["force_sync"].isNull() && doc["force_sync"].as<bool>()) {
        tm.forceNtpSync();
    }

    // 返回最新时间状态（复用 GET handler）
    return time_data_get_handler(req);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  /api/wifi/* — WiFi 扫描/连接/状态
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
esp_err_t WebServer::wifi_list_handler(httpd_req_t *req) {
    std::string list = WifiModule::get_wifi_list();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, list.c_str());
    return ESP_OK;
}

esp_err_t WebServer::wifi_scan_handler(httpd_req_t *req) {
    // 触发新一轮扫描（阻塞约 2-3 秒）
    WifiModule::refresh_wifi_list();
    std::string list = WifiModule::get_wifi_list();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, list.c_str());
    return ESP_OK;
}

esp_err_t WebServer::wifi_connect_handler(httpd_req_t *req) {
    std::string body = read_post_body(req);
    if (body.empty()) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }

    JsonDocument doc;
    if (deserializeJson(doc, body)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    const char *ssid = doc["ssid"];
    if (!ssid) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ssid");
        return ESP_FAIL;
    }

    const char *password = doc["password"] | "";
    bool success = WifiModule::connect_to_new_ap(ssid, password);

    JsonDocument resp;
    resp["success"] = success;
    if (!success) {
        // 获取失败原因
        std::string status = WifiModule::get_status_json();
        JsonDocument status_doc;
        deserializeJson(status_doc, status);
        resp["error"] = status_doc["error"] | "UNKNOWN";
    }

    std::string output;
    serializeJson(resp, output);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, output.c_str());
    return ESP_OK;
}

esp_err_t WebServer::wifi_status_handler(httpd_req_t *req) {
    std::string status = WifiModule::get_status_json();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, status.c_str());
    return ESP_OK;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  /api/reset — 恢复出厂设置
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
esp_err_t WebServer::reset_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, R"({"message":"Resetting to factory defaults..."})");

    // 延迟 500ms 确保 HTTP 响应发出后再重启
    vTaskDelay(pdMS_TO_TICKS(500));
    Controller::factoryReset(); // 永不返回（会 reboot）
    return ESP_OK;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  API 路由注册
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void WebServer::register_api_routes(httpd_handle_t server) {
    // 管子/氖泡设置
    httpd_uri_t light_get = {
        .uri = "/api/light_data", .method = HTTP_GET,
        .handler = light_data_get_handler, .user_ctx = nullptr
    };
    httpd_uri_t light_post = {
        .uri = "/api/light_data", .method = HTTP_POST,
        .handler = light_data_post_handler, .user_ctx = nullptr
    };

    // 时间设置
    httpd_uri_t time_get = {
        .uri = "/api/time_data", .method = HTTP_GET,
        .handler = time_data_get_handler, .user_ctx = nullptr
    };
    httpd_uri_t time_post = {
        .uri = "/api/time_data", .method = HTTP_POST,
        .handler = time_data_post_handler, .user_ctx = nullptr
    };

    // WiFi
    httpd_uri_t wifi_list = {
        .uri = "/api/wifi/list", .method = HTTP_GET,
        .handler = wifi_list_handler, .user_ctx = nullptr
    };
    httpd_uri_t wifi_scan = {
        .uri = "/api/wifi/scan", .method = HTTP_POST,
        .handler = wifi_scan_handler, .user_ctx = nullptr
    };
    httpd_uri_t wifi_connect = {
        .uri = "/api/wifi/connect", .method = HTTP_POST,
        .handler = wifi_connect_handler, .user_ctx = nullptr
    };
    httpd_uri_t wifi_status = {
        .uri = "/api/wifi/status", .method = HTTP_GET,
        .handler = wifi_status_handler, .user_ctx = nullptr
    };

    // 恢复出厂设置
    httpd_uri_t reset = {
        .uri = "/api/reset", .method = HTTP_POST,
        .handler = reset_handler, .user_ctx = nullptr
    };

    httpd_register_uri_handler(server, &light_get);
    httpd_register_uri_handler(server, &light_post);
    httpd_register_uri_handler(server, &time_get);
    httpd_register_uri_handler(server, &time_post);
    httpd_register_uri_handler(server, &wifi_list);
    httpd_register_uri_handler(server, &wifi_scan);
    httpd_register_uri_handler(server, &wifi_connect);
    httpd_register_uri_handler(server, &wifi_status);
    httpd_register_uri_handler(server, &reset);

    ESP_LOGI(TAG, "9 API routes registered");
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  初始化
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
httpd_handle_t WebServer::init() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard; // 启用通配符匹配
    config.server_port = 80;
    config.max_uri_handlers = 12; // 9 API + 1 静态文件 + 余量
    config.stack_size = 8192; // ArduinoJson 需要更大栈空间
    config.lru_purge_enable = true; // 开启 LRU 清除机制，防止长期运行 Socket 泄露导致服务器卡死

    httpd_handle_t server = nullptr;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // 1. 先注册所有精确匹配的 API 路由
        register_api_routes(server);

        // 2. 最后注册通配符静态文件路由（兜底）
        httpd_uri_t file_serve = {
            .uri = "/*", // 通配符，匹配所有剩下的 GET 请求
            .method = HTTP_GET,
            .handler = file_get_handler,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(server, &file_serve);

        ESP_LOGI(TAG, "HTTP server started successfully");
        return server;
    }

    ESP_LOGE(TAG, "Error starting server!");
    return nullptr;
}
