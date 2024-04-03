#include "web_server.h"
#include "wifi_module.h"
#include "nvs_module.h"
#include "littlefs_module.h"
#include "in12_driver.h"
#include "display_module.h"


void main_thread(void *pVoid) {
    // 初始化硬件驱动程序74hc595
    IN12_DRIVER::init();
    // 初始化NVS
    NVSModule::init_nvs();
    // 初始化LittleFS
    littleFS::init();
    // 初始化wifi
    WifiModule::init();
    // Start the web server
    WebServer::start();
    // Start the clock main loop
    DisplayModule::clock_main_loop();

    vTaskDelete(nullptr);
}

extern "C" void app_main() {
    xTaskCreate(main_thread, "main_thread", 4096, nullptr, 5, nullptr);
}



