#include "in12_driver.h"

gpio_num_t IN12_DRIVER::ST_CP = GPIO_NUM_19;
gpio_num_t IN12_DRIVER::SH_CP = GPIO_NUM_18;
gpio_num_t IN12_DRIVER::DS = GPIO_NUM_5;


void IN12_DRIVER::init() {
    gpio_set_direction(ST_CP, GPIO_MODE_OUTPUT);
    gpio_set_direction(SH_CP, GPIO_MODE_OUTPUT);
    gpio_set_direction(DS, GPIO_MODE_OUTPUT);
    gpio_set_level(ST_CP, 0);
    gpio_set_level(SH_CP, 0);
    gpio_set_level(DS, 0);
    driver_74hc595("0000000000000000");
}

void IN12_DRIVER::driver_74hc595(const std::string &data) {
    for (auto &x: data) {
        if (x == '1') {
            gpio_set_level(DS, 1);
            gpio_set_level(SH_CP, 1);
            gpio_set_level(SH_CP, 0);
            gpio_set_level(DS, 0);
        } else {
            gpio_set_level(DS, 0);
            gpio_set_level(SH_CP, 1);
            gpio_set_level(SH_CP, 0);
        }
    }
    gpio_set_level(ST_CP, 1);
    gpio_set_level(ST_CP, 0);
}

void IN12_DRIVER::driver_set_clock(const std::string &dec_data) {
    std::string dec_data_all;
    for (auto &i: dec_data) {
        std::string binary = std::bitset<4>(i - '0').to_string();
        reverse(binary.begin(), binary.end());
        dec_data_all += binary;
    }
    driver_74hc595(dec_data_all);
}

void IN12_DRIVER::driver_off_clock() {
    driver_74hc595("1111111111111111");
}

void IN12_DRIVER::flush_clock() {  // 总用时 1s
    for (int i = 0; i < 10; i++) {
        std::string data_all;
        for (int j = 0; j < 4; j++) {
            data_all += std::to_string(i);
        }
        driver_set_clock(data_all);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

