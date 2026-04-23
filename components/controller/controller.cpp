//
// Created by HAIRONG ZHU on 2026/4/21.
//

#include "controller.h"
#include "nvs_module.h"
#include "project_config.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <ctime>

static const char *TAG = "Controller";

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  单例
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Controller &Controller::getInstance() {
    static Controller instance;
    return instance;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  初始化
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void Controller::init() {
    mutex_ = xSemaphoreCreateMutex();

    // 1. 硬件初始化
    IN12_DRIVER::init();

    // 2. 从 NVS 恢复用户设置
    loadFromNVS();

    // 3. 创建防阴极中毒定时器（暂不启动）
    esp_timer_create_args_t timer_args = {};
    timer_args.callback = poisonTimerCb;
    timer_args.arg = this;
    timer_args.name = "poison";
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &poison_timer_));

    // 4. 注册 TimeManager 回调（先注册，再 init，确保回调不丢）
    auto &tm = TimeManager::getInstance();
    tm.registerHalfSecondTick([this](bool is_first_half) {
        onHalfSecond(is_first_half);
    });
    tm.registerMinuteTick([this](int h, int m) {
        onMinuteChange(h, m);
    });

    // 5. 启动 TimeManager（默认 CST-8, NTP 自动校准）
    tm.init("CST-8");

    // 6. 首次刷新
    resolveDisplay();
    resolveNeon();

    ESP_LOGI(TAG, "Controller initialized successfully");
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  核心决策引擎
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void Controller::resolveDisplay() {
    // ── Priority 3: 防阴极中毒动画（最高优先级）──
    if (poison_active_) {
        IN12_DRIVER::display_digits(poison_digit_, poison_digit_,
                                    poison_digit_, poison_digit_);
        return;
    }

    // ── 总开关 ──
    if (!settings_.tube_on) {
        IN12_DRIVER::display_off();
        return;
    }

    // ── Priority 2: 展示模式（手动数字覆盖时钟，系统时钟后台继续走）──
    if (settings_.display_mode) {
        IN12_DRIVER::display_digits(settings_.display_d1, settings_.display_d2,
                                    settings_.display_d3, settings_.display_d4);
        return;
    }

    // ── Priority 1: 熄灯时段 ──
    if (settings_.sleep_enabled && isInSleepWindow(cur_hour_, cur_minute_)) {
        IN12_DRIVER::display_off();
        return;
    }

    // ── Priority 0: 时钟模式（默认）──
    IN12_DRIVER::display_digits(cur_hour_ / 10, cur_hour_ % 10,
                                cur_minute_ / 10, cur_minute_ % 10);
}

void Controller::resolveNeon() {
    // 氖泡独立于显示模式，始终根据时钟秒来闪烁
    if (!settings_.neon_on) {
        IN12_DRIVER::neon_set(false);
        return;
    }
    if (settings_.neon_blink) {
        IN12_DRIVER::neon_set(neon_phase_);
        return;
    }
    // 不闪烁则常亮
    IN12_DRIVER::neon_set(true);
}

bool Controller::isInSleepWindow(int h, int m) const {
    int now = h * 60 + m;
    int start = settings_.sleep_start_h * 60 + settings_.sleep_start_m;
    int end = settings_.sleep_end_h * 60 + settings_.sleep_end_m;

    if (start <= end) {
        // 不跨午夜: 如 01:00 ~ 06:00
        return now >= start && now < end;
    } else {
        // 跨午夜: 如 23:00 ~ 07:00
        return now >= start || now < end;
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  TimeManager 回调
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void Controller::onHalfSecond(bool is_first_half) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    neon_phase_ = is_first_half;
    resolveNeon();
    xSemaphoreGive(mutex_);
}

void Controller::onMinuteChange(int hour, int minute) {
    xSemaphoreTake(mutex_, portMAX_DELAY);

    cur_hour_ = hour;
    cur_minute_ = minute;

    // ── 防阴极中毒触发检查 (仅在整点、且动画未运行时) ──
    if (!poison_active_ && minute == 0) {
        // 固定时间模式优先（时长更长）
        if (settings_.fixed_poison_enabled && hour == settings_.fixed_poison_hour) {
            // 每轮 0→9 用 100ms×10 = 1秒，持续 N 秒 = N×10 ticks
            startAntiPoison(settings_.fixed_poison_duration * 10);
        }
        // 整点模式（仅单次 0→9, 共 10 ticks = 1秒）
        else if (settings_.hourly_poison_enabled) {
            startAntiPoison(10);
        }
    }

    resolveDisplay();
    xSemaphoreGive(mutex_);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  防阴极中毒动画
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void Controller::startAntiPoison(int total_ticks) {
    poison_active_ = true;
    poison_digit_ = 0;
    poison_tick_count_ = 0;
    poison_total_ticks_ = total_ticks;

    ESP_LOGI(TAG, "Anti-poison started: %d ticks (%d seconds)",
             total_ticks, total_ticks / 10);

    // 启动 100ms 周期定时器
    esp_timer_start_periodic(poison_timer_, 100 * 1000); // 100ms = 100000μs
    resolveDisplay(); // 立刻显示第一个数字 0
}

void Controller::stopAntiPoison() {
    esp_timer_stop(poison_timer_);
    poison_active_ = false;
    ESP_LOGI(TAG, "Anti-poison finished");
    resolveDisplay(); // 恢复正常显示
}

void Controller::poisonTimerCb(void *arg) {
    auto *self = static_cast<Controller *>(arg);
    xSemaphoreTake(self->mutex_, portMAX_DELAY);

    self->poison_tick_count_++;
    // 始终从 0 开始循环: 0,1,2,...,9, 0,1,2,...
    self->poison_digit_ = self->poison_tick_count_ % 10;

    if (self->poison_tick_count_ >= self->poison_total_ticks_) {
        self->stopAntiPoison();
    } else {
        self->resolveDisplay();
    }

    xSemaphoreGive(self->mutex_);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  设置管理
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void Controller::applySettings(const LightSettings &newSettings) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    settings_ = newSettings;
    saveToNVS();
    resolveDisplay();
    resolveNeon();
    xSemaphoreGive(mutex_);
}

const LightSettings &Controller::getSettings() const {
    return settings_;
}

void Controller::forceTimeRefresh() {
    xSemaphoreTake(mutex_, portMAX_DELAY);

    time_t now;
    time(&now);
    struct tm timeinfo{};
    localtime_r(&now, &timeinfo);

    cur_hour_ = timeinfo.tm_hour;
    cur_minute_ = timeinfo.tm_min;

    resolveDisplay();
    xSemaphoreGive(mutex_);
}

void Controller::factoryReset() {
    ESP_LOGW(TAG, "Factory reset! Erasing NVS and rebooting...");
    nvs_flash_erase();
    esp_restart();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  NVS 持久化
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void Controller::loadFromNVS() {
    LightSettings tmp{};
    esp_err_t err = NVSModule::load(NVS_NS_LIGHT, NVS_KEY_LIGHT, tmp);
    if (err == ESP_OK) {
        settings_ = tmp;
        ESP_LOGI(TAG, "Light settings loaded from NVS");
    } else {
        ESP_LOGW(TAG, "No saved light settings or size mismatch (%s), using defaults",
                 esp_err_to_name(err));
    }
}

void Controller::saveToNVS() {
    esp_err_t err = NVSModule::save(NVS_NS_LIGHT, NVS_KEY_LIGHT, settings_);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Light settings saved to NVS");
    } else {
        ESP_LOGE(TAG, "Failed to save light settings to NVS: %s", esp_err_to_name(err));
    }
}
