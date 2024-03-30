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
    static bool is_adjusted;   // 时间是否已经校准
    static int hour_current;
    static int minute_current;
    static int second_current;
    static char user_timezone[8];


    static void set_system_timezone();

    static void get_time();

    static string get_fill_zero_time();

    static void check_sync(); // 可以更新class的时间

    static void set_user_timezone(const char *timezone);

};


#endif //IN12_TIMEZONE_MODULE_H
