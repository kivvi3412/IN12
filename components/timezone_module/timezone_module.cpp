//
// Created by HAIRONG ZHU on 2024/3/30.
//

#include "timezone_module.h"


bool TimezoneModule::is_adjusted = false;   // 时间是否已经校准
int TimezoneModule::hour_current = 0;
int TimezoneModule::minute_current = 0;
int TimezoneModule::second_current = 0;
char TimezoneModule::user_timezone[8] = "CST-8";


string s_fill_zero(const string &input_string) {
    return string(2 - input_string.length(), '0') + input_string;
}

void TimezoneModule::set_system_timezone() {
    esp_sntp_stop();  // 停止 SNTP 客户端

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com");
    esp_netif_sntp_init(&config);


    setenv("TZ", user_timezone, 1);
    tzset();

    esp_sntp_init();  // 重新启动 SNTP 客户端
}

void TimezoneModule::get_time() {
    time_t now;
    struct tm struct_tm{};
    time(&now);
    localtime_r(&now, &struct_tm);
    hour_current = struct_tm.tm_hour;
    minute_current = struct_tm.tm_min;
    second_current = struct_tm.tm_sec;
}

string TimezoneModule::get_fill_zero_time() {
    return s_fill_zero(to_string(hour_current)) + s_fill_zero(to_string(minute_current));
}

void TimezoneModule::check_sync() { // check_sync 会更新时间
    auto is_sync = sntp_get_sync_status();
    if (is_sync == SNTP_SYNC_STATUS_COMPLETED) {
        is_adjusted = true;
    }
    get_time();
}

void TimezoneModule::set_user_timezone(const char *timezone) {
    strcpy(user_timezone, timezone);
    is_adjusted = false;
    set_system_timezone();
}
