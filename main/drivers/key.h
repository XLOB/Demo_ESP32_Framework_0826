/**
 * @file key.h
 * @brief 按键驱动
 *
 * 提供 GPIO 按键的软件消抖和按下事件回调。
 * 使用轮询方式检测，需周期性调用 Key_poll()。
 */
#ifndef KEY_H
#define KEY_H

#include <stdint.h>

struct Device;

/** 按键事件回调类型 */
typedef void (*key_event_cb_t)(void);

/** 按键设备私有数据 */
struct Key {
    int           gpio_num;            ///< GPIO 编号
    int           state;               ///< 当前稳定电平（0=按下，1=松开）
    int           pressed;             ///< 是否处于按下状态（仅按下沿置 1）

    key_event_cb_t on_pressed;         ///< 按下事件回调

    int           debounce_count;      ///< 当前连续相同电平计数
    int           debounce_threshold;  ///< 消抖阈值（连续相同电平次数）
};

/**
 * @brief 创建一个按键设备
 * @param gpio_num  GPIO 编号
 * @param name      设备名称（唯一标识）
 * @return 设备指针，失败返回 NULL
 *
 * 注意：返回的设备通过 malloc 分配，注册到框架后无需手动释放。
 *       当前项目中按键为永久设备，未实现销毁接口。
 */
struct Device *Key_create(int gpio_num, const char *name);

/**
 * @brief 设置按键按下回调
 * @param dev  按键设备指针
 * @param cb   回调函数（可为 NULL 表示取消）
 */
void Key_set_callback(struct Device *dev, key_event_cb_t cb);

/**
 * @brief 轮询按键状态（需周期性调用，建议 20ms 一次）
 * @param dev  按键设备指针
 *
 * 内部进行软件消抖和边沿检测，检测到按下沿时触发回调。
 */
void Key_poll(struct Device *dev);

#endif /* KEY_H */
