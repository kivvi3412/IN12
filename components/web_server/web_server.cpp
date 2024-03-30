#include <string>
#include "web_server.h"

#include "cJSON.h"
#include "timezone_module.h"
#include "display_module.h"

static const char *webserver_TAG = "webserver";


static esp_err_t common_get_handler(httpd_req_t *req) {
    // 获取请求的 URI
    const char *uri = req->uri;

    // 如果 URI 是根目录，则默认为 index.html
    if (strcmp(uri, "/") == 0) {
        uri = "/index.html";
    }

    // 从 littleFS 中读取文件内容
    std::string content = littleFS::readFile(uri + 1); // 跳过前导斜杠

    if (!content.empty()) {
        // 文件存在，设置响应内容类型并发送文件内容
        httpd_resp_send(req, content.c_str(), HTTPD_RESP_USE_STRLEN);
    } else {
        // 文件不存在，返回 404 错误页面
        std::string notFoundContent = littleFS::readFile("404NotFound.html");
        if (!notFoundContent.empty()) {
            httpd_resp_set_status(req, "404 Not Found");
            httpd_resp_set_type(req, "text/html");
            httpd_resp_send(req, notFoundContent.c_str(), HTTPD_RESP_USE_STRLEN);
        } else {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "404 File not found");
        }
    }
    return ESP_OK;
}

static esp_err_t save_wifi_config_post_handler(httpd_req_t *req) {
    char buf[128];
    int ret, remaining = req->content_len;

    // Read the request content into the buffer
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf + req->content_len - remaining,
                                  MIN(remaining, sizeof(buf) - req->content_len + remaining))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                httpd_resp_send_408(req);
            }
            return ESP_FAIL;
        }
        remaining -= ret;
    }
    buf[req->content_len] = '\0';

    // 开始解析 JSON 数据
    char ssid[33] = {0};
    char password[65] = {0};
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    httpd_resp_set_type(req, "application/json");

    cJSON *root = cJSON_Parse(buf);
    if (root == nullptr) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != nullptr) {
            ESP_LOGI(webserver_TAG, "JSON parsing error before: %s", error_ptr);
        }
        // 返回标准json错误 {"code": 500, "message": "Invalid JSON data", "data": "buf"}, http状态码为200
        cJSON *json_response = cJSON_CreateObject();
        cJSON_AddNumberToObject(json_response, "code", 500);
        cJSON_AddStringToObject(json_response, "message", "Invalid JSON data");
        cJSON_AddStringToObject(json_response, "data", buf);
        httpd_resp_set_status(req, HTTPD_200);
        httpd_resp_sendstr(req, cJSON_Print(json_response));
        cJSON_Delete(json_response);
        return ESP_FAIL;
    }

    cJSON *json_ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON *json_password = cJSON_GetObjectItem(root, "password");
    if (cJSON_IsString(json_ssid) && (json_ssid->valuestring != nullptr) &&
        cJSON_IsString(json_password) && (json_password->valuestring != nullptr)) {    // 判断是否为字符串
        strncpy(ssid, json_ssid->valuestring, sizeof(ssid) - 1);
        strncpy(password, json_password->valuestring, sizeof(password) - 1);
        strncpy((char *) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char *) wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    } else {
        ESP_LOGI(webserver_TAG, "Invalid JSON data: %s", buf);
        // 返回标准json错误 {"code": 500, "message": "Invalid JSON data", "data": "buf"}, http状态码为200
        cJSON *json_response = cJSON_CreateObject();
        cJSON_AddNumberToObject(json_response, "code", 500);
        cJSON_AddStringToObject(json_response, "message", "ssid or password is not a string");
        cJSON_AddStringToObject(json_response, "data", buf);
        httpd_resp_set_status(req, HTTPD_200);
        httpd_resp_sendstr(req, cJSON_Print(json_response));
        cJSON_Delete(json_response);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // 保存到 NVS
    esp_err_t err = NVSModule::save_wifi_config(&wifi_config);
    if (err != ESP_OK) {
        // 返回标准json错误 {"code": 500, "message": "Failed to save WiFi config", "data": "buf"}, http状态码为200
        cJSON *json_response = cJSON_CreateObject();
        cJSON_AddNumberToObject(json_response, "code", 500);
        cJSON_AddStringToObject(json_response, "message", "Failed to save WiFi config");
        cJSON_AddStringToObject(json_response, "data", buf);
        httpd_resp_set_status(req, HTTPD_200);
        httpd_resp_sendstr(req, cJSON_Print(json_response));
        cJSON_Delete(json_response);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // 打印保存的配置
    wifi_config_t loaded_wifi_config;
    NVSModule::load_wifi_config(&loaded_wifi_config);
    ESP_LOGI(webserver_TAG, "Saved WiFi config: SSID: %s, Password: %s", loaded_wifi_config.sta.ssid,
             loaded_wifi_config.sta.password);

    // 返回标准json成功 {"code": 200, "message": "WiFi configuration saved successfully", "data": "buf"}, http状态码为200
    cJSON *json_response = cJSON_CreateObject();
    cJSON_AddNumberToObject(json_response, "code", 200);
    cJSON_AddStringToObject(json_response, "message", "WiFi configuration saved successfully");
    cJSON_AddStringToObject(json_response, "data", buf);
    httpd_resp_set_status(req, HTTPD_200);
    httpd_resp_sendstr(req, cJSON_Print(json_response));
    cJSON_Delete(json_response);
    cJSON_Delete(root);

    // 等待 1s, 不然消息还没发回去就断开连接了
    vTaskDelay(200 / portTICK_PERIOD_MS);

    // STA 的重新连接
    WifiModule::staReconnect();

    return ESP_OK;
}

static esp_err_t set_common_data_post_handler(httpd_req_t *req) {
    char buf[256];
    int ret, remaining = req->content_len;

    // Read the request content into the buffer
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf + req->content_len - remaining,
                                  MIN(remaining, sizeof(buf) - req->content_len + remaining))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                httpd_resp_send_408(req);
            }
            return ESP_FAIL;
        }
        remaining -= ret;
    }
    buf[req->content_len] = '\0';

    // 开始解析 JSON 数据
    cJSON *root = cJSON_Parse(buf);
    if (root == nullptr) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != nullptr) {
            ESP_LOGI(webserver_TAG, "JSON parsing error before: %s", error_ptr);
        }
        // 返回标准json错误 {"code": 500, "message": "Invalid JSON data", "data": "buf"}, http状态码为200
        cJSON *json_response = cJSON_CreateObject();
        cJSON_AddNumberToObject(json_response, "code", 500);
        cJSON_AddStringToObject(json_response, "message", "Invalid JSON data");
        cJSON_AddStringToObject(json_response, "data", buf);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, HTTPD_200);
        httpd_resp_sendstr(req, cJSON_Print(json_response));
        cJSON_Delete(json_response);
        return ESP_FAIL;
    }

    // 获取 mode 字段
    cJSON *json_mode = cJSON_GetObjectItem(root, "mode");
    if (json_mode == nullptr || !cJSON_IsNumber(json_mode)) {
        // 返回标准json错误 {"code": 500, "message": "mode field is required and must be a number", "data": "buf"}, http状态码为200
        cJSON *json_response = cJSON_CreateObject();
        cJSON_AddNumberToObject(json_response, "code", 500);
        cJSON_AddStringToObject(json_response, "message", "mode field is required and must be a number");
        cJSON_AddStringToObject(json_response, "data", buf);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, HTTPD_200);
        httpd_resp_sendstr(req, cJSON_Print(json_response));
        cJSON_Delete(json_response);
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    int mode = json_mode->valueint;

    DisplayModule::presentation_mode = mode;
    if (mode == 1) {  // 自动时钟模式
        cJSON *json_timezone = cJSON_GetObjectItem(root, "timezone");
        if (json_timezone == nullptr || !cJSON_IsString(json_timezone)) {
            // 返回标准json错误 {"code": 500, "message": "timezone field is required and must be a string", "data": "buf"}, http状态码为200
            cJSON *json_response = cJSON_CreateObject();
            cJSON_AddNumberToObject(json_response, "code", 500);
            cJSON_AddStringToObject(json_response, "message", "timezone field is required and must be a string");
            cJSON_AddStringToObject(json_response, "data", buf);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_status(req, HTTPD_200);
            httpd_resp_sendstr(req, cJSON_Print(json_response));
            cJSON_Delete(json_response);
            cJSON_Delete(root);
            return ESP_FAIL;
        }
        const char *timezone = json_timezone->valuestring;
        TimezoneModule::set_user_timezone(timezone);  // 设置时区
        // 返回标准json成功 {"code": 200, "message": "Timezone set successfully", "data": timezone}, http状态码为200
        cJSON *json_response = cJSON_CreateObject();
        cJSON_AddNumberToObject(json_response, "code", 200);
        cJSON_AddStringToObject(json_response, "message", "Auto clock mode set successfully");
        cJSON_AddStringToObject(json_response, "data", timezone);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, HTTPD_200);
        httpd_resp_sendstr(req, cJSON_Print(json_response));
        cJSON_Delete(json_response);
    } else if (mode == 2) {  // 自定义数字模式
        cJSON *json_custom_number = cJSON_GetObjectItem(root, "custom_number");
        if (json_custom_number == nullptr || !cJSON_IsString(json_custom_number) ||
            strlen(json_custom_number->valuestring) != 4) {
            // 返回标准json错误 {"code": 500, "message": "custom_number field is required, must be a string and length must be 4", "data": "buf"}, http状态码为200
            cJSON *json_response = cJSON_CreateObject();
            cJSON_AddNumberToObject(json_response, "code", 500);
            cJSON_AddStringToObject(json_response, "message",
                                    "custom_number field is required, must be a string and length must be 4");
            cJSON_AddStringToObject(json_response, "data", buf);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_status(req, HTTPD_200);
            httpd_resp_sendstr(req, cJSON_Print(json_response));
            cJSON_Delete(json_response);
            cJSON_Delete(root);
            return ESP_FAIL;
        }
        const char *custom_number = json_custom_number->valuestring;
        DisplayModule::custom_data = custom_number;  // 设置自定义数字
        // 返回标准json成功 {"code": 200, "message": "Custom number set successfully", "data": custom_number}, http状态码为200
        cJSON *json_response = cJSON_CreateObject();
        cJSON_AddNumberToObject(json_response, "code", 200);
        cJSON_AddStringToObject(json_response, "message", "Custom number set successfully");
        cJSON_AddStringToObject(json_response, "data", custom_number);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, HTTPD_200);
        httpd_resp_sendstr(req, cJSON_Print(json_response));
        cJSON_Delete(json_response);
    } else {
        // 返回标准json错误 {"code": 500, "message": "Invalid mode", "data": "buf"}, http状态码为200
        cJSON *json_response = cJSON_CreateObject();
        cJSON_AddNumberToObject(json_response, "code", 500);
        cJSON_AddStringToObject(json_response, "message", "Invalid mode");
        cJSON_AddStringToObject(json_response, "data", buf);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, HTTPD_200);
        httpd_resp_sendstr(req, cJSON_Print(json_response));
        cJSON_Delete(json_response);
    }

    // 设置防阴极中毒时间
    cJSON *json_flush_time = cJSON_GetObjectItem(root, "flush_time");
    if (json_flush_time != nullptr && cJSON_IsString(json_flush_time) && strlen(json_flush_time->valuestring) == 4) {
        const char *flush_time = json_flush_time->valuestring;
        DisplayModule::set_flush_clock_time(flush_time);  // 设置防阴极中毒时间
        // 返回标准json成功 {"code": 200, "message": "Flush time set successfully", "data": flush_time}, http状态码为200
        cJSON *json_response = cJSON_CreateObject();
        cJSON_AddNumberToObject(json_response, "code", 200);
        cJSON_AddStringToObject(json_response, "message", "Flush time set successfully");
        cJSON_AddStringToObject(json_response, "data", flush_time);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, HTTPD_200);
        httpd_resp_sendstr(req, cJSON_Print(json_response));
        cJSON_Delete(json_response);
    }

    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t get_common_data_get_handler(httpd_req_t *req) {
    // 创建JSON对象
    cJSON *root = cJSON_CreateObject();
    // 添加mode字段
    cJSON_AddNumberToObject(root, "mode", DisplayModule::presentation_mode);
    // 添加timezone字段
    cJSON_AddStringToObject(root, "timezone", TimezoneModule::user_timezone);
    // 添加custom_number字段
    cJSON_AddStringToObject(root, "custom_number", DisplayModule::get_custom_data().c_str());
    // 添加flush_time字段
    cJSON_AddStringToObject(root, "flush_time", DisplayModule::get_flush_clock_time().c_str());
    // 添加current_display_number字段
    cJSON_AddStringToObject(root, "current_display_number", DisplayModule::get_current_presentation_data().c_str());
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, HTTPD_200);
    httpd_resp_sendstr(req, cJSON_Print(root));
    // 释放内存
    cJSON_Delete(root);

    return ESP_OK;
}

httpd_handle_t WebServer::start() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard; // 使用通配符匹配 URI
    config.server_port = 80;
    httpd_handle_t server = nullptr;

    // Start the httpd server
    ESP_LOGI(webserver_TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // 根目录
        static const httpd_uri_t ap_uri_get = {
                .uri       = "/*",
                .method    = HTTP_GET,
                .handler   = common_get_handler
        };
        httpd_register_uri_handler(server, &ap_uri_get);

        // API 保存 WiFi 配置
        static const httpd_uri_t save_wifi_config_uri_post = {
                .uri = "/api/save_wifi_config",
                .method = HTTP_POST,
                .handler = save_wifi_config_post_handler
        };
        httpd_register_uri_handler(server, &save_wifi_config_uri_post);

        /*
         * 时区设置 （立刻同步时间按钮)
         * 自定义数字模式， 输入四个数字
         * 设置防阴极中毒时间
         */
        static const httpd_uri_t set_timezone_uri_post = {
                .uri = "/api/set_common_data",
                .method = HTTP_POST,
                .handler = set_common_data_post_handler
        };
        httpd_register_uri_handler(server, &set_timezone_uri_post);

        /*
         * 获取当前显示内容
         * 获取当前时间
         * 获取当前时区
         * 获取当前防阴极中毒时间
         */
        static const httpd_uri_t get_current_data_uri_get = {
                .uri = "/api/get_common_data",
                .method = HTTP_POST,
                .handler = get_common_data_get_handler
        };
        httpd_register_uri_handler(server, &get_current_data_uri_get);

        return server;
    }

    ESP_LOGI(webserver_TAG, "Error starting server!");
    return nullptr;
}
