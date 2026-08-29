/**
 * @file key.c
 * @brief 按键驱动实现
 *
 * 采用软件消抖：连续 N 次采样电平相同才认为状态稳定。
 * 仅在按下沿（高→低）触发回调，松开沿不触发。
 */
#include "key.h"
#include "../framework/framework.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include <stdlib.h>
#include <string.h>

static const char *TAG = "key";

/** 默认消抖阈值：3 次采样稳定，轮询周期 20ms → 约 60ms 消抖 */
#define KEY_DEFAULT_DEBOUNCE  3

/* ------------------------------------------------------------------ */
/* 设备操作函数                                                       */
/* ------------------------------------------------------------------ */

static int key_init(void *self)
{
    struct Key *key = (struct Key *)self;

    key->pressed            = 0;
    key->on_pressed         = NULL;
    key->debounce_count     = 0;
    key->debounce_threshold = KEY_DEFAULT_DEBOUNCE;

    /* 配置 GPIO 为输入上拉 */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << key->gpio_num),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    /* 读取初始状态 */
    key->state = gpio_get_level(key->gpio_num);

    ESP_LOGI(TAG, "按键初始化完成，GPIO=%d", key->gpio_num);
    return 0;
}

static int key_read(void *self, void *buf, size_t len)
{
    struct Key *key = (struct Key *)self;
    if (len < sizeof(int))
        return -1;

    key->state = gpio_get_level(key->gpio_num);
    memcpy(buf, &key->state, sizeof(int));
    return sizeof(int);
}

static int key_write(void *self, const void *buf, size_t len)
{
    (void)self; (void)buf; (void)len;
    return -1; /* 按键为只读设备 */
}

static int key_deinit(void *self)
{
    (void)self;
    return 0;
}

static const struct DeviceOps key_ops = {
    .init   = key_init,
    .read   = key_read,
    .write  = key_write,
    .deinit = key_deinit,
};

/* ------------------------------------------------------------------ */
/* 公开 API                                                           */
/* ------------------------------------------------------------------ */

struct Device *Key_create(int gpio_num, const char *name)
{
    struct Key *key = calloc(1, sizeof(struct Key));
    if (!key)
        return NULL;

    key->gpio_num = gpio_num;

    struct Device *dev = calloc(1, sizeof(struct Device));
    if (!dev) {
        free(key);
        return NULL;
    }

    dev->name = name;
    dev->data = key;
    dev->ops  = &key_ops;

    return dev;
}

void Key_set_callback(struct Device *dev, key_event_cb_t cb)
{
    if (!dev || !dev->data)
        return;

    struct Key *key = (struct Key *)dev->data;
    key->on_pressed = cb;
}

void Key_poll(struct Device *dev)
{
    if (!dev || !dev->data)
        return;

    struct Key *key = (struct Key *)dev->data;

    int cur = gpio_get_level(key->gpio_num);

    if (cur != key->state) {
        /* 电平变化，计数 */
        key->debounce_count++;

        if (key->debounce_count >= key->debounce_threshold) {
            /* 连续 N 次相同，确认状态改变 */
            key->state          = cur;
            key->debounce_count = 0;

            if (cur == 0) {
                /* 按下沿 */
                key->pressed = 1;
                if (key->on_pressed)
                    key->on_pressed();
            } else {
                /* 松开沿 */
                key->pressed = 0;
            }
        }
    } else {
        /* 电平未变，重置计数 */
        key->debounce_count = 0;
    }
}
