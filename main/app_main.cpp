#include "esp_log.h"

#include "nvs_module.h"
#include "littlefs_module.h"
#include "wifi_module.h"
#include "controller.h"
#include "web_server.h"

static auto TAG = "MAIN";

extern "C" void app_main() {
    // 1. 基础存储
    ESP_ERROR_CHECK(NVSModule::init_nvs());
    littleFS::init();

    // 2. 网络层（启动 AP 热点 + STA 自动连接已保存的 WiFi）
    WifiModule::init();

    // 3. 中央控制器（初始化硬件 → 加载NVS设置 → 注册TimeManager回调 → 启动时间服务）
    vTaskDelay(pdMS_TO_TICKS(2000));    // 等WIFI启动防止浪涌电流
    Controller::getInstance().init();

    // 4. Web 服务（注册API路由 + 静态文件服务）
    WebServer::init();

    ESP_LOGI(TAG, "═══ IN12 Nixie Clock System Started ═══");
}
