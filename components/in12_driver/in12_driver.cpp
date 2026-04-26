//
// Created by HAIRONG ZHU on 2026/4/20.
//

#include "in12_driver.h"

void IN12_DRIVER::init() {
    gpio_set_direction(ST_CP, GPIO_MODE_OUTPUT);
    gpio_set_direction(SH_CP, GPIO_MODE_OUTPUT);
    gpio_set_direction(DS, GPIO_MODE_OUTPUT);
    gpio_set_direction(NEON, GPIO_MODE_OUTPUT);
    gpio_set_direction(HV_CTL, GPIO_MODE_OUTPUT);

    gpio_set_level(ST_CP, 0);
    gpio_set_level(SH_CP, 0);
    gpio_set_level(DS, 0);
    gpio_set_level(NEON, 0);

    write_595(0x0000); // 初始化显示 0000
    gpio_set_level(HV_CTL, 1); // 默认开机开启高压
}

// 核心：使用移位和位与操作，直接推送 16 bit 数据
void IN12_DRIVER::write_595(uint16_t data) {
    // 从最高位 (MSB, 第15位) 开始提取，一直到最低位 (LSB, 第0位)
    for (int i = 15; i >= 0; --i) {
        // (data >> i) & 1 会精准提取出当前位是 0 还是 1
        gpio_set_level(DS, (data >> i) & 1);

        // 产生移位时钟脉冲
        gpio_set_level(SH_CP, 1);
        gpio_set_level(SH_CP, 0);
    }
    // 产生锁存时钟脉冲，输出数据
    gpio_set_level(ST_CP, 1);
    gpio_set_level(ST_CP, 0);
}

// 接收四个数字 (0-9)，直接拼装成 16 位机器码
void IN12_DRIVER::display_digits(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4) {
    // 保护机制，防止越界访问数组
    d1 %= 10;
    d2 %= 10;
    d3 %= 10;
    d4 %= 10;

    // 利用查表法获取翻转后的 BCD 码，并通过移位操作拼装成 16 位整数
    uint16_t payload = (BCD_LUT[d1] << 12) |
                       (BCD_LUT[d2] << 8) |
                       (BCD_LUT[d3] << 4) |
                       (BCD_LUT[d4]);

    write_595(payload);
}

void IN12_DRIVER::display_off() {
    write_595(0xFFFF); // 16个1，二进制全为1，即15，译码器将息屏
}

void IN12_DRIVER::neon_set(const bool on) {
    gpio_set_level(NEON, on ? 1 : 0);
}

void IN12_DRIVER::neon_toggle() {
    int current_state = gpio_get_level(NEON);
    gpio_set_level(NEON, !current_state); // 取反并输出
}

void IN12_DRIVER::hv_enable(bool enable) {
    gpio_set_level(HV_CTL, enable ? 1 : 0);
}
