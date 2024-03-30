//
// Created by HAIRONG ZHU on 2024/3/30.
//

#ifndef IN12_DISPLAY_MODULE_H
#define IN12_DISPLAY_MODULE_H
#include <string>
#include "in12_driver.h"
#include "timezone_module.h"
#include "wifi_module.h"

class DisplayModule {
public:
    static std::string flush_time;
    static std::string custom_data;
    static int presentation_mode; // 1: 自动时钟模式 2: 自定义数字模式
    static std::string current_presentation_data;


    static void set_flush_clock_time(std::string f_time);
    static std::string get_flush_clock_time();
    static std::string get_custom_data();
    static void set_display_number(const std::string &number);
    static void clock_main_loop();
    static std::string get_current_presentation_data();

};


#endif //IN12_DISPLAY_MODULE_H
