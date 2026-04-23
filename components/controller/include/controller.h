//
// Created by HAIRONG ZHU on 2026/4/21.
//

#ifndef IN12_CONTROLLER_H
#define IN12_CONTROLLER_H

#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "in12_driver.h"
#include "timezone_module.h"

/*
 *  设置氖泡是否闪烁（一秒一次）
 *  设置几点到几点让辉光管关闭，例如晚上睡觉关灯
 *  设置防阴极中毒时间，以及刷新时间
 *  设置是否整点防阴极中毒刷新，以及刷新时间
 *  
 *  设置手动开关辉光管
 *  展示模式可以手动设置任意数字
 *  
 *  
 *  
 *  > 分两种模式：时钟模式 和 展示模式 (二选一，开了时钟模式就是时钟的界面，不开就是展示模式，两者界面不可混在一起)
 *  网页前端发送和返回的信息:
 *  管子发送/接收信息(nvs储存):
 *      氖泡/辉光管 on/off
 *      氖泡闪烁on/off
 *      熄灯时间段on/off 开启时间，关闭时间
 *      固定时间防阴极中毒on/off 几点刷新，刷新持续时间
 *      整点防阴极中毒on/off 刷新持续时间
 *      展示模式of/off 任意数字
 *      
 *  时区/WIFI设置:
 *      时区时间分为浏览器输入时间手动校准和NTP时区校准
 *      NTP或手动校准 on/off (默认NTP)
 *          NTPon: WIFISSID(列表选择) WIFI密码(wifi单独存储过了) WI-FI状态 立即扫描WIFI 连接WIFI
 *                 输入时区(默认东8)， 立刻校准button
 *          手动校准: 输入当前时间(前端展示是当前系统时间) 保存
 *      
 *  恢复出厂设置
 */

/*
 *  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *   显示优先级（高 → 低）:
 *     3. 防阴极中毒动画 (临时接管)
 *     2. 展示模式 (手动数字覆盖时钟)
 *     1. 熄灯时段 (管子关闭)
 *     0. 时钟模式 (默认 HH:MM)
 *
 *   管子总开关 tube_on=false → 无条件 display_off
 *   氖泡独立控制，不受显示模式影响
 *  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 */

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  NVS 持久化设置结构体
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
struct LightSettings {
    // ── 管子与氖泡 ──
    bool tube_on          = true;   // 辉光管总开关
    bool neon_on          = true;   // 氖泡开关
    bool neon_blink       = true;   // 氖泡闪烁（半秒亮半秒灭）

    // ── 定时熄灯 ──
    bool sleep_enabled    = false;
    uint8_t sleep_start_h = 23;     // 开始熄灯（时）
    uint8_t sleep_start_m = 0;      // 开始熄灯（分）
    uint8_t sleep_end_h   = 7;      // 结束熄灯（时）
    uint8_t sleep_end_m   = 0;      // 结束熄灯（分）

    // ── 固定时间防阴极中毒 (大半夜长时间刷) ──
    bool fixed_poison_enabled      = true;
    uint8_t fixed_poison_hour      = 4;    // 几点执行
    uint16_t fixed_poison_duration = 120;  // 持续秒数 (0→9循环, 每轮100ms×10=1秒)

    // ── 整点防阴极中毒 (每小时刷一遍) ──
    bool hourly_poison_enabled     = true;
    // 固定行为: 0→9 刷一遍, 100ms/位, 共1秒, 无需配置时长

    // ── 展示模式 ──
    bool display_mode     = false;  // 开启后管子显示自定义数字, 系统时钟后台继续走
    uint8_t display_d1    = 1;      // 四位自定义数字
    uint8_t display_d2    = 2;
    uint8_t display_d3    = 3;
    uint8_t display_d4    = 4;
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Controller — 中央决策单例
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
class Controller {
public:
    static Controller& getInstance();
    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;

    /// 系统启动时调用: 初始化硬件、加载NVS、注册TimeManager回调、启动时间服务
    void init();

    /// Web API 调用: 应用新设置 → 立即刷新显示 + NVS持久化
    void applySettings(const LightSettings& newSettings);

    /// Web API 调用: 获取当前设置的只读引用
    const LightSettings& getSettings() const;

    /// 手动时间/时区变更后调用: 强制从RTC重读时间并刷新显示
    void forceTimeRefresh();

    /// 恢复出厂设置: 清除NVS, 重启
    static void factoryReset();

private:
    Controller() = default;

    LightSettings settings_;
    SemaphoreHandle_t mutex_ = nullptr;

    // ── 运行时状态（不持久化）──
    int cur_hour_    = 0;
    int cur_minute_  = 0;
    bool neon_phase_ = false;       // 当前氖泡相位 (true=亮)

    bool poison_active_      = false;
    uint8_t poison_digit_    = 0;   // 当前显示的数字 0-9
    int poison_tick_count_   = 0;   // 已执行 tick 数
    int poison_total_ticks_  = 0;   // 目标 tick 数
    esp_timer_handle_t poison_timer_ = nullptr;

    // ── 核心决策引擎 ──
    void resolveDisplay();                    // 优先级决策: 此刻该显示什么?
    void resolveNeon();                       // 氖泡决策: 此刻该亮/灭/闪?
    bool isInSleepWindow(int h, int m) const; // 当前是否在熄灯窗口内

    // ── TimeManager 回调 (mutex 保护) ──
    void onHalfSecond(bool is_first_half);
    void onMinuteChange(int hour, int minute);

    // ── 防阴极中毒动画 (调用者持有 mutex) ──
    void startAntiPoison(int total_ticks);    // total_ticks = 10 整点 | duration*10 固定
    void stopAntiPoison();
    static void poisonTimerCb(void* arg);     // esp_timer 100ms 回调

    // ── NVS 读写 ──
    void loadFromNVS();
    void saveToNVS();
};

#endif //IN12_CONTROLLER_H
