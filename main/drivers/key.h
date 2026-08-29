#ifndef KEY_H
#define KEY_H

#include <stdint.h>

struct Device;

// 按键事件回调类型
typedef void (*key_event_cb_t)(void);

struct Key
{
    int gpio_num;
    int state;
    int pressed;

    key_event_cb_t on_pressed; // 按下事件回调
};

struct Device *Key_create(int gpio_num, const char *name);

// 设置按键按下回调
void Key_set_callback(struct Device *dev, key_event_cb_t cb);

// 轮询按键，内部进行状态机检测并触发回调
void Key_poll(struct Device *dev);

#endif