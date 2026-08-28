#include "key.h"
#include "../framework/framework.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "key";

// 全局按键私有数据
static struct Key g_key;

/*
 * 按键初始化：
 * 1. 把 A 键的 GPIO 配置成输入
 * 2. 读取一次初始状态，存进 key->state
 */
static int key_init(void *self)
{
    struct Key *key = (struct Key *)self;

    // 设置这个按键对应的 GPIO
    key->gpio_num = 46;

    // 配置 GPIO46 为输入模式
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << key->gpio_num),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, // 启用上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // 初始状态
    key->state = gpio_get_level(key->gpio_num);

    ESP_LOGI(TAG, "按键初始化完成，GPIO=%d，初始状态=%d",
             key->gpio_num, key->state);

    return 0;
}

/*
 * 读取按键状态：
 * 1. 调用 gpio_get_level 读取当前电平
 * 2. 保存到 key->state
 * 3. 同时把状态写进用户的 buf 里
 */
static int key_read(void *self, void *buf, size_t len)
{
    struct Key *key = (struct Key *)self;

    if (len < sizeof(int))
        return -1;

    key->state = gpio_get_level(key->gpio_num);

    *(int *)buf = key->state;

    return sizeof(int);
}

/*
 * 按键不支持写操作。
 * 所以直接返回 -1。
 */
static int key_write(void *self, const void *buf, size_t len)
{
    // 按键是输入设备，不能写
    return -1;
}

/*
 * 销毁设备。按键没有需要释放的资源。
 */
static int key_deinit(void *self)
{
    return 0;
}

/*
 * 按键的操作表：
 * 告诉框架：按键支持哪些操作。
 */
static const struct DeviceOps key_ops = {
    .init = key_init,
    .read = key_read,
    .write = key_write,
    .deinit = key_deinit,
};

/*
 * 按键设备对象：
 * 这是框架认识的“按键设备”。
 */
static struct Device g_key_device = {
    .name = "key_a",
    .data = &g_key,
    .ops = &key_ops,
};

/*
 * 提供给外部的接口：
 * 返回按键设备对象的地址。
 */
struct Device *Key_get_device(void)
{
    return &g_key_device;
}
