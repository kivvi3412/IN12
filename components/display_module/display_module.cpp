//
// Created by HAIRONG ZHU on 2024/3/30.
//

#include "display_module.h"

const char *DISPLAY_TAG = "DisplayModule";

void DisplayModule::set_flush_clock_time(std::string f_time) {
    DisplayModule::flush_time = std::move(f_time);
}

std::string DisplayModule::get_flush_clock_time() {
    return flush_time;
}

std::string DisplayModule::get_custom_data() {
    return DisplayModule::custom_data;
}

std::string DisplayModule::get_current_presentation_data() {
    return DisplayModule::current_presentation_data;
}

// 设置显示数字
void DisplayModule::set_display_number(const std::string &number) {
    current_presentation_data = number;
    IN12_DRIVER::driver_set_clock(number);
}

// 主线程
void DisplayModule::clock_main_loop() {
    std::string old_time = "0000";  // 用于判断时间是否变化, 变化则刷新
    while (true) {
        // 到时间自动刷新防止阴极中毒
        if (DisplayModule::flush_time == TimezoneModule::get_fill_zero_time()) {
            for (int i = 0; i < 60; i++) {
                IN12_DRIVER::flush_clock();
                ESP_LOGI(DISPLAY_TAG, "Flush clock");
            }
        }
        if (DisplayModule::presentation_mode == 1) { // 自动时钟模式
            TimezoneModule::check_sync();
            if (WifiModule::is_sta_connected && TimezoneModule::is_adjusted) {
                if (old_time != TimezoneModule::get_fill_zero_time()) {
                    old_time = TimezoneModule::get_fill_zero_time();
                    ESP_LOGI(DISPLAY_TAG, "Time: %s", old_time.c_str());
                }
                DisplayModule::set_display_number(old_time); // 防止切换模式无法正确显示
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            } else {
                ESP_LOGI(DISPLAY_TAG, "Wifi Status: %s, time is adjusted: %d",
                         WifiModule::print_sta_disconnected_reason(WifiModule::disconnected_reason),
                         TimezoneModule::is_adjusted);
                IN12_DRIVER::flush_clock(); // 也会延迟一秒
            }
        } else if (DisplayModule::presentation_mode == 2) {
            // webserver 调用接口直接设置显示的数字
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        } else {
            ESP_LOGI(DISPLAY_TAG, "Time: %s", TimezoneModule::get_fill_zero_time().c_str());
            IN12_DRIVER::flush_clock(); // 也会延迟一秒
        }
    }
}