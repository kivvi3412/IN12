//
// Created by HAIRONG ZHU on 2024/3/30.
//

#include "display_module.h"

std::string DisplayModule::flush_time = "0430";
std::string DisplayModule::custom_data = "0000"; // 自定义显示的数字
std::string DisplayModule::current_presentation_data = "0000";  // 当前输出的数字
int DisplayModule::presentation_mode = 1;

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
    while (true) {
        // 到时间自动刷新防止阴极中毒
        if (DisplayModule::flush_time == TimezoneModule::get_fill_zero_time()) {
            for (int i = 0; i < 60; i++) {
                IN12_DRIVER::flush_clock();
                ESP_LOGI("DisplayModule", "Flush clock");
            }
        }
        if (DisplayModule::presentation_mode == 1) { // 自动时钟模式
            TimezoneModule::check_sync();
            if (WifiModule::wifi_is_connected() && TimezoneModule::is_adjusted) {
                std::string old_time = TimezoneModule::get_fill_zero_time();
                DisplayModule::set_display_number(old_time);
                ESP_LOGI("DisplayModule", "Time: %s", old_time.c_str());
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            } else {
                ESP_LOGI("DisplayModule", "wifi is connected: %d, time is adjusted: %d",
                         WifiModule::wifi_is_connected(),
                         TimezoneModule::is_adjusted);
                IN12_DRIVER::flush_clock(); // 也会延迟一秒
            }
        } else if (DisplayModule::presentation_mode == 2) {
            DisplayModule::set_display_number(DisplayModule::get_custom_data());
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        } else {
            ESP_LOGI("DisplayModule", "Time: %s", TimezoneModule::get_fill_zero_time().c_str());
            IN12_DRIVER::flush_clock(); // 也会延迟一秒
        }
    }
}