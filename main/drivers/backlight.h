#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include <stdint.h>
#include "driver/ledc.h"

struct Device;

// 背光使用的LEDC配置宏
#define BACKLIGHT_LEDC_MODE LEDC_LOW_SPEED_MODE
#define BACKLIGHT_LEDC_TIMER LEDC_TIMER_0
#define BACKLIGHT_LEDC_CHANNEL LEDC_CHANNEL_0
#define BACKLIGHT_GPIO 16

struct Backlight
{
    uint8_t brightness; // 0~100
};

struct Device *Backlight_get_device(void);

#endif