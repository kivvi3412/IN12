//
// Created by HAIRONG ZHU on 2026/4/19.
//

#ifndef IN12_TIMEZONE_MODULE_H
#define IN12_TIMEZONE_MODULE_H


#include <string>
#include <functional>
#include <esp_sntp.h>

class TimeManager {
public:
    // 定义回调函数的类型
    // 半秒级回调：用于控制氖泡闪烁 (传入 true 代表前半秒点亮，false 代表后半秒熄灭)
    using HalfSecondCallback = std::function<void(bool is_on)>;
    // 分钟级回调：用于刷新辉光管，提供数字类型和补零后的字符串 (如 "0830")
    using MinuteCallback = std::function<void(int hour, int minute)>;

    // 单例模式：全局唯一实例
    static TimeManager &getInstance() {
        static TimeManager instance;
        return instance;
    }

    // 删除拷贝构造和赋值操作，确保单例的安全性
    TimeManager(const TimeManager &) = delete;

    TimeManager &operator=(const TimeManager &) = delete;

    // 1. 初始化系统时间与网络同步任务
    void init(const char *timezone = "CST-8");

    // 2. 注册外设驱动回调
    void registerHalfSecondTick(HalfSecondCallback callback);

    void registerMinuteTick(MinuteCallback callback);

    // 3. NTP 与状态管理
    void setNtpMode(bool enable); // 开启或关闭 NTP 自动校准
    void forceNtpSync(); // 强制立即发起一次 NTP 校准
    [[nodiscard]] bool isNtpSynced() const; // 获取当前是否已经成功对时
    [[nodiscard]] std::string getLastSyncTimeStr() const; // 获取最后一次成功对时的时间字符串 (网页展示用)

    // 4. 手动时间设置 (用于无网络时的纯手动调表)
    static void setManualTime(int year, int month, int day, int hour, int minute, int second);

private:
    TimeManager() = default;

    ~TimeManager() = default;

    HalfSecondCallback onHalfSecondTick = nullptr;
    MinuteCallback onMinuteTick = nullptr;

    int last_minute = -1;
    bool last_is_first_half = false;

    time_t last_sync_time = 0;
    bool is_ntp_synced = false;

    // FreeRTOS 守护任务
    static void timeTask(void *pvParameters);

    // ESP-IDF 底层触发的 NTP 同步成功回调
    static void time_sync_notification_cb(timeval *tv);
};


#endif //IN12_TIMEZONE_MODULE_H
