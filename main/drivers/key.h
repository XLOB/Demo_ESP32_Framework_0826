#ifndef KEY_H
#define KEY_H

#include <stdint.h>

struct Device;
typedef void (*key_event_cb_t)(void);

struct Key
{
    int gpio_num;
    int state;
    int pressed; // 用于状态机

    key_event_cb_t on_pressed; // 回调函数
};

// 获取按键设备
struct Device *Key_get_device(void);
void Key_set_callback(struct Device *dev, key_event_cb_t cb);
void Key_poll(struct Device *dev);

#endif