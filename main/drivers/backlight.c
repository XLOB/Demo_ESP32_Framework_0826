#include "backlight.h"
#include "../framework/framework.h"

#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "backlight";

static struct Backlight g_backlight;

static int backlight_init(void *self)
{
    struct Backlight *bl = (struct Backlight *)self;

    // 1. 配置 LEDC 定时器
    ledc_timer_config_t timer_cfg = {
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = BACKLIGHT_LEDC_TIMER,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK};
    ledc_timer_config(&timer_cfg);

    // 2. 配置 LEDC 通道
    ledc_channel_config_t channel_cfg = {
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .channel = BACKLIGHT_LEDC_CHANNEL,
        .timer_sel = BACKLIGHT_LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = BACKLIGHT_GPIO,
        .duty = 255, // 初始全亮
        .hpoint = 0};
    ledc_channel_config(&channel_cfg);

    bl->brightness = 100;

    ESP_LOGI(TAG, "背光驱动初始化完成");
    return 0;
}

static int backlight_write(void *self, const void *buf, size_t len)
{
    struct Backlight *bl = (struct Backlight *)self;

    if (len < sizeof(uint8_t))
        return -1;

    uint8_t percent = *(const uint8_t *)buf;
    if (percent > 100)
        percent = 100;

    uint32_t duty = (percent * 255) / 100;
    ledc_set_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, duty);
    ledc_update_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL);

    bl->brightness = percent;

    return sizeof(uint8_t);
}

static int backlight_read(void *self, void *buf, size_t len)
{
    struct Backlight *bl = (struct Backlight *)self;

    if (len < sizeof(uint8_t))
        return -1;
    memcpy(buf, &bl->brightness, sizeof(uint8_t));
    return sizeof(uint8_t);
}

static int backlight_deinit(void *self)
{
    ledc_stop(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, 0);
    return 0;
}

static const struct DeviceOps backlight_ops = {
    .init = backlight_init,
    .read = backlight_read,
    .write = backlight_write,
    .deinit = backlight_deinit,
};

static struct Device g_backlight_device = {
    .name = "backlight",
    .data = &g_backlight,
    .ops = &backlight_ops,
};

struct Device *Backlight_get_device(void)
{
    return &g_backlight_device;
}