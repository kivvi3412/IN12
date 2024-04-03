#ifndef IN12_IN12_DRIVER_H
#define IN12_IN12_DRIVER_H

#include "driver/gpio.h"
#include "string"
#include <bitset>
#include <algorithm>
#include "freertos/freeRTOS.h"
#include "freertos/task.h"
#include "display_module.h"



class IN12_DRIVER {
public:
    inline static gpio_num_t ST_CP = GPIO_NUM_19;
    inline static gpio_num_t SH_CP = GPIO_NUM_18;
    inline static gpio_num_t DS = GPIO_NUM_5;


    static void init();
    static void driver_74hc595(const std::string &data);
    static void driver_set_clock(const std::string &dec_data);
    static void driver_off_clock();
    static void flush_clock();
};

#endif