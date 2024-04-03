//
// Created by HAIRONG ZHU on 2024/3/30.
//

#ifndef IN12_TIMEZONE_MODULE_H
#define IN12_TIMEZONE_MODULE_H

#include <ctime>
#include <esp_sntp.h>
#include "esp_netif_sntp.h"
#include <string>
#include <esp_log.h>


using namespace std;

class TimezoneModule {
public:
    inline static bool is_adjusted = false;   // 时间是否已经校准
    inline static int hour_current = 0;
    inline static int minute_current = 0;
    inline static int second_current = 0;
    inline static char user_timezone[8] = "CST-8";


    static void set_system_timezone();

    static void get_time();

    static string get_fill_zero_time();

    static void check_sync(); // 可以更新class的时间

    static void set_user_timezone(const char *timezone);

};


#endif //IN12_TIMEZONE_MODULE_H
