#ifndef KEY_H
#define KEY_H

#include <stdint.h>

struct Device;

struct Key
{
    int gpio_num; // 这个按键接在哪个 GPIO
    int state;    // 当前状态：0 或 1
};

// 获取按键设备
struct Device *Key_get_device(void);

#endif