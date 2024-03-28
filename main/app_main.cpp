#include "web_server.h"
#include "wifi_module.h"
#include "nvs_module.h"


void main_thread(void *pVoid) {
    // 初始化NVS
    NVSModule::init_nvs();
    // 初始化wifi
    WifiModule::init();
    // Start the web server
    WebServer::start();

    vTaskDelete(nullptr);
}

extern "C" void app_main() {
    xTaskCreate(main_thread, "main_thread", 4096, nullptr, 5, nullptr);
}



