#include "web_server.h"



static const char *webserver_TAG = "webserver";

// Utility function to convert a single hex digit character to its integer value
unsigned char hexCharToVal(unsigned char c) {
    if ('0' <= c && c <= '9') return (unsigned char)(c - '0');
    if ('a' <= c && c <= 'f') return (unsigned char)(c - 'a' + 10);
    if ('A' <= c && c <= 'F') return (unsigned char)(c - 'A' + 10);
    return 0;
}

// URL decode a string in place
void urlDecode(char *str) {
    char *dest = str;
    while (*str) {
        if (*str == '%') {
            if (*(str + 1) && *(str + 2) && isxdigit(*(str + 1)) && isxdigit(*(str + 2))) {
                *dest = (char)(hexCharToVal(*(str + 1)) * 16 + hexCharToVal(*(str + 2)));
                str += 3;
            } else {
                // 如果百分号后不是两个有效的十六进制数字，则跳过
                str++;
            }
        } else if (*str == '+') {
            *dest = ' ';
            str++;
        } else {
            *dest = *str;
            str++;
        }
        dest++;
    }
    *dest = '\0'; // 确保解码后的字符串以空字符终止
}



static esp_err_t webserver_get_handler(httpd_req_t *req) {
    const char resp[] = "<!DOCTYPE html>\n"
                        "<html>\n"
                        "<head>\n"
                        "    <title>WiFi Configuration</title>\n"
                        "</head>\n"
                        "<body>\n"
                        "    <h1>WiFi Configuration</h1>\n"
                        "    <form action=\"/save_wifi_config\" method=\"post\">\n"
                        "        <label for=\"ssid\">SSID:</label><br>\n"
                        "        <input type=\"text\" id=\"ssid\" name=\"ssid\"><br>\n"
                        "        <label for=\"password\">Password:</label><br>\n"
                        "        <input type=\"password\" id=\"password\" name=\"password\"><br><br>\n"
                        "        <input type=\"submit\" value=\"Save\">\n"
                        "    </form> \n"
                        "</body>\n"
                        "</html>";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// 请勿修改此代码否则随时可能会暴毙
static esp_err_t save_wifi_config_post_handler(httpd_req_t *req) {
    char buf[512];
    int ret, remaining = req->content_len;

    // Read the request content into the buffer
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf + req->content_len - remaining, MIN(remaining, sizeof(buf) - req->content_len + remaining))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                httpd_resp_send_408(req);
            }
            return ESP_FAIL;
        }
        remaining -= ret;
    }

    // Null-terminate the buffer
    buf[req->content_len] = '\0';

    // Find the SSID and password in the request content
    char *ssid_start = strstr(buf, "ssid=");
    char *password_start = strstr(buf, "password=");
    if (ssid_start == NULL || password_start == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID or Password");
        return ESP_FAIL;
    }

    char ssid[33] = {0};
    char password[65] = {0};

    ssid_start += 5;  // Skip "ssid="
    char *ssid_end = strchr(ssid_start, '&');
    if (ssid_end == nullptr) {
        ssid_end = buf + req->content_len;  // SSID is at the end of the content
    }
    strncpy(ssid, ssid_start, MIN(sizeof(ssid) - 1, ssid_end - ssid_start));

    password_start += 9;  // Skip "password="
    strncpy(password, password_start, MIN(sizeof(password) - 1, buf + req->content_len - password_start));

    // URL decode SSID and password
    urlDecode(ssid);
    urlDecode(password);

    // Print the decoded SSID and password
    ESP_LOGI(webserver_TAG, "Decoded SSID: %s, Password: %s", ssid, password);

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));

    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    esp_err_t err = NVSModule::save_wifi_config(&wifi_config);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save WiFi config");
        return ESP_FAIL;
    }

    // Print the saved configuration
    wifi_config_t loaded_wifi_config;
    NVSModule::load_wifi_config(&loaded_wifi_config);
    ESP_LOGI(webserver_TAG, "Saved WiFi config: SSID: %s, Password: %s", loaded_wifi_config.sta.ssid,
             loaded_wifi_config.sta.password);

    httpd_resp_sendstr(req, "WiFi configuration saved successfully");
    // STA 的重新连接
    WifiModule::staReconnect();
    return ESP_OK;
}

static const httpd_uri_t ap_uri_get = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = webserver_get_handler
};

static const httpd_uri_t save_wifi_config_uri_post = {
        .uri = "/save_wifi_config",
        .method = HTTP_POST,
        .handler = save_wifi_config_post_handler
};

httpd_handle_t WebServer::start() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    httpd_handle_t server = nullptr;

    // Start the httpd server
    ESP_LOGI(webserver_TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(webserver_TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &ap_uri_get);
        httpd_register_uri_handler(server, &save_wifi_config_uri_post);
        return server;
    }

    ESP_LOGI(webserver_TAG, "Error starting server!");
    return nullptr;
}