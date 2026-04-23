//
// Created by HAIRONG ZHU on 2026/4/19.
//

#include "timezone_module.h"
#include <esp_log.h>
#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "TimeManager";

void TimeManager::time_sync_notification_cb(timeval *tv) {
    TimeManager &manager = TimeManager::getInstance();
    manager.is_ntp_synced = true;
    time(&manager.last_sync_time); // 记录同步成功时的绝对时间戳
    ESP_LOGI(TAG, "NTP Time Sync Completed!");
}

void TimeManager::init(const char *timezone) {
    // 1. 设置时区
    setenv("TZ", timezone, 1);
    tzset();

    // 2. 配置 SNTP
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "pool.ntp.org");

    // 注册同步成功的回调函数
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    // 启动 SNTP 后台服务
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP Initialized, waiting for sync...");

    // 3. 启动高精度时间分发任务
    // 分配 4096 字节栈空间，优先级设为 5 (相对较高，保证秒闪烁不被普通任务阻塞)
    xTaskCreate(timeTask, "TimeTask", 4096, this, 5, nullptr);
}

void TimeManager::registerHalfSecondTick(HalfSecondCallback callback) {
    onHalfSecondTick = std::move(callback);
}

void TimeManager::registerMinuteTick(MinuteCallback callback) {
    onMinuteTick = std::move(callback);
}

void TimeManager::setNtpMode(bool enable) {
    if (enable) {
        if (!esp_sntp_enabled()) {
            ESP_LOGI(TAG, "Starting NTP Service...");
            esp_sntp_init();
        }
    } else {
        if (esp_sntp_enabled()) {
            ESP_LOGI(TAG, "Stopping NTP Service, entering manual mode.");
            esp_sntp_stop();
            is_ntp_synced = false;
        }
    }
}

void TimeManager::forceNtpSync() {
    ESP_LOGI(TAG, "Forcing NTP Sync...");
    if (esp_sntp_enabled()) {
        esp_sntp_stop();
    }
    is_ntp_synced = false;
    esp_sntp_init(); // 重新拉起会立即触发一次同步请求
}

bool TimeManager::isNtpSynced() const {
    return is_ntp_synced;
}

std::string TimeManager::getLastSyncTimeStr() const {
    if (last_sync_time == 0) return "Not Synced Yet";

    tm timeinfo{};
    localtime_r(&last_sync_time, &timeinfo);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return buf;
}

void TimeManager::setManualTime(int year, int month, int day, int hour, int minute, int second) {
    tm t{};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = second;

    time_t timeSinceEpoch = mktime(&t);
    timeval now = {.tv_sec = timeSinceEpoch, .tv_usec = 0};

    // 强制修改 ESP32 底层硬件 RTC 时间
    settimeofday(&now, nullptr);
    ESP_LOGI(TAG, "Manual time set to: %04d-%02d-%02d %02d:%02d:%02d",
             year, month, day, hour, minute, second);
}

void TimeManager::timeTask(void *pvParameters) {
    auto *manager = static_cast<TimeManager *>(pvParameters);
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (true) {
        // 读取系统底层 RTC 时间 (获取微秒级别精度)
        timeval tv{};
        gettimeofday(&tv, nullptr);
        tm timeinfo{};
        localtime_r(&tv.tv_sec, &timeinfo);

        // 1. 触发分钟级回调 (控制辉光管)
        if (timeinfo.tm_min != manager->last_minute) {
            manager->last_minute = timeinfo.tm_min;
            if (manager->onMinuteTick) {
                manager->onMinuteTick(timeinfo.tm_hour, timeinfo.tm_min);
            }
        }

        // 2. 触发半秒级回调 (控制氖泡)
        bool is_first_half = (tv.tv_usec < 500000);
        if (is_first_half != manager->last_is_first_half) {
            manager->last_is_first_half = is_first_half;
            if (manager->onHalfSecondTick) {
                manager->onHalfSecondTick(is_first_half);
            }
        }

        // 绝对精准延时，周期 100ms (10Hz)，避免累积误差导致漏秒
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
    }
}
