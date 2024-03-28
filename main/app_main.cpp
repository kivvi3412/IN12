#include "web_server.h"
#include "wifi_module.h"
#include "nvs_module.h"


extern "C" void app_main() {
    // 初始化NVS
    NVSModule::init_nvs();
    // 初始化wifi
    WifiModule::init();
    // Start the web server
    WebServer::start();
}



