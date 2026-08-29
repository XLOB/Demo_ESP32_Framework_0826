#include "key.h"
#include "../framework/framework.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include <stdlib.h>
#include <string.h>

static const char *TAG = "key";

static int key_init(void *self)
{
    struct Key *key = (struct Key *)self;

    key->pressed = 0;
    key->on_pressed = NULL;
    key->debounce_count = 0;
    key->debounce_threshold = 3; // 默认3次采样稳定，约20ms*3=60ms

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << key->gpio_num),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

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
    return -1;
}

static int key_deinit(void *self)
{
    return 0;
}

static const struct DeviceOps key_ops = {
    .init = key_init,
    .read = key_read,
    .write = key_write,
    .deinit = key_deinit,
};

struct Device *Key_create(int gpio_num, const char *name)
{
    struct Key *key = calloc(1, sizeof(struct Key));
    if (!key)
        return NULL;

    key->gpio_num = gpio_num;

    struct Device *dev = calloc(1, sizeof(struct Device));
    if (!dev)
    {
        free(key);
        return NULL;
    }

    dev->name = name;
    dev->data = key;
    dev->ops = &key_ops;

    return dev;
}

// 设置回调
void Key_set_callback(struct Device *dev, key_event_cb_t cb)
{
    if (!dev || !dev->data)
        return;

    struct Key *key = (struct Key *)dev->data;
    key->on_pressed = cb;
}

// 轮询按键，带软件消抖，检测按下沿并触发回调
void Key_poll(struct Device *dev)
{
    if (!dev || !dev->data)
        return;

    struct Key *key = (struct Key *)dev->data;

    int cur = gpio_get_level(key->gpio_num);

    if (cur != key->state)
    {
        key->debounce_count++;
        if (key->debounce_count >= key->debounce_threshold)
        {
            key->state = cur;
            key->debounce_count = 0;

            // 按下沿
            if (cur == 0)
            {
                key->pressed = 1;
                if (key->on_pressed)
                {
                    key->on_pressed();
                }
            }
            // 松开沿
            else
            {
                key->pressed = 0;
            }
        }
    }
    else
    {
        key->debounce_count = 0;
    }
}