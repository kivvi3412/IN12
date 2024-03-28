#include "web_server.h"
#include "wifi_module.h"
#include "nvs_module.h"


extern "C" void app_main() {
    ESP_ERROR_CHECK(init_nvs()); // 初始化NVS
    // 初始化wifi
    wifi_init();
    // Start the web server
    start_webserver();
}



