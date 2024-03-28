#include <string>
#include "web_server.h"
#include "cJSON.h"

static const char *webserver_TAG = "webserver";


static esp_err_t webserver_get_handler(httpd_req_t *req) {
    std::string resp = littleFS::readFile("index.html");
    httpd_resp_send(req, resp.c_str(), HTTPD_RESP_USE_STRLEN);
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


httpd_handle_t WebServer::start() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    httpd_handle_t server = nullptr;

    // Start the httpd server
    ESP_LOGI(webserver_TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // 根目录
        static const httpd_uri_t ap_uri_get = {
                .uri       = "/",
                .method    = HTTP_GET,
                .handler   = webserver_get_handler
        };
        httpd_register_uri_handler(server, &ap_uri_get);

        // API 保存 WiFi 配置
        static const httpd_uri_t save_wifi_config_uri_post = {
                .uri = "/api/save_wifi_config",
                .method = HTTP_POST,
                .handler = save_wifi_config_post_handler
        };
        httpd_register_uri_handler(server, &save_wifi_config_uri_post);
        return server;
    }

    ESP_LOGI(webserver_TAG, "Error starting server!");
    return nullptr;
}
