//
// Created by HAIRONG ZHU on 2026/4/20.
//

#ifndef IN12_IN12_DRIVER_H
#define IN12_IN12_DRIVER_H

#include "driver/gpio.h"
#include "project_config.h"

class IN12_DRIVER {
public:
    static constexpr gpio_num_t ST_CP = ST_CP_GPIO;
    static constexpr gpio_num_t SH_CP = SH_CP_GPIO;
    static constexpr gpio_num_t DS = DS_GPIO;
    static constexpr gpio_num_t NEON = NEON_GPIO;
    static constexpr gpio_num_t HV_CTL = HV_CTL_GPIO;

    static void init();

    static void display_digits(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4);

    static void display_off();

    static void neon_set(bool on);

    static void neon_toggle();

    static void hv_enable(bool enable);

private:
    static void write_595(uint16_t data);

    // 查表法：直接将 0-9 映射到翻转后的 4位 BCD 码
    static constexpr uint8_t BCD_LUT[10] = {
        0b0000, // 0 的翻转
        0b1000, // 1 的翻转 (原为 0001)
        0b0100, // 2 的翻转 (原为 0010)
        0b1100, // 3 的翻转 (原为 0011)
        0b0010, // 4 的翻转 (原为 0100)
        0b1010, // 5 的翻转 (原为 0101)
        0b0110, // 6 的翻转 (原为 0110)
        0b1110, // 7 的翻转 (原为 0111)
        0b0001, // 8 的翻转 (原为 1000)
        0b1001 // 9 的翻转 (原为 1001)
    };
};


#endif //IN12_IN12_DRIVER_H
