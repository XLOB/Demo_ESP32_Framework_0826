/**
 * @file backlight.h
 * @brief LCD 背光驱动（LEDC PWM）
 *
 * 通过 LEDC PWM 控制 LCD 背光亮度，范围 0~100%。
 */
#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include <stdint.h>

struct Device;

/* ===== 硬件配置 ===== */

#define BACKLIGHT_GPIO         16
#define BACKLIGHT_LEDC_MODE    LEDC_LOW_SPEED_MODE
#define BACKLIGHT_LEDC_TIMER   LEDC_TIMER_0
#define BACKLIGHT_LEDC_CHANNEL LEDC_CHANNEL_0
#define BACKLIGHT_PWM_FREQ_HZ  5000   /* PWM 频率 5kHz，避免人耳可闻 */

/** 背光设备私有数据 */
struct Backlight {
    uint8_t brightness;  ///< 当前亮度（0~100）
};

/**
 * @brief 获取背光设备实例
 * @return 设备指针
 */
struct Device *Backlight_get_device(void);

#endif /* BACKLIGHT_H */
