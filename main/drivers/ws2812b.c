#include "ws2812b.h"
#include "../framework/framework.h"
#include "led_strip.h"
#include "esp_log.h"

static const char *TAG = "ws2812b";

// 全局 LED 灯条句柄
static led_strip_handle_t g_led_strip = NULL;

// 设备私有数据
static struct ws2812b g_ws2812b;

static int ws2812b_init(void *self)
{
    struct ws2812b *ws2812b = (struct ws2812b *)self;

    ws2812b->red = 0;
    ws2812b->green = 0;
    ws2812b->blue = 0;

    led_strip_config_t strip_config = {
        .strip_gpio_num = 41,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false,
        },
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &g_led_strip));

    ESP_LOGI(TAG, "WS2812B 初始化完成");

    return 0;
}

static int ws2812b_read(void *self, void *buf, size_t len)
{
    struct ws2812b *ws2812b = (struct ws2812b *)self;

    if (len < 3)
        return -1;

    uint8_t *rgb = (uint8_t *)buf;
    rgb[0] = ws2812b->red;
    rgb[1] = ws2812b->green;
    rgb[2] = ws2812b->blue;

    return 3;
}

static int ws2812b_write(void *self, const void *buf, size_t len)
{
    struct ws2812b *ws2812b = (struct ws2812b *)self;

    if (len < 3)
        return -1;

    const uint8_t *rgb = (const uint8_t *)buf;
    ws2812b->red = rgb[0];
    ws2812b->green = rgb[1];
    ws2812b->blue = rgb[2];

    led_strip_set_pixel(g_led_strip, 0,
                        ws2812b->red,
                        ws2812b->green,
                        ws2812b->blue);
    led_strip_refresh(g_led_strip);

    return 3;
}

static int ws2812b_deinit(void *self)
{
    if (g_led_strip != NULL)
    {
        led_strip_del(g_led_strip);
        g_led_strip = NULL;
    }

    return 0;
}

static const struct DeviceOps ws2812b_ops = {
    .init = ws2812b_init,
    .read = ws2812b_read,
    .write = ws2812b_write,
    .deinit = ws2812b_deinit,
};

static struct Device g_ws2812b_device = {
    .name = "ws2812b",
    .data = &g_ws2812b,
    .ops = &ws2812b_ops,
};

struct Device *Ws2812b_get_device(void)
{
    return &g_ws2812b_device;
}