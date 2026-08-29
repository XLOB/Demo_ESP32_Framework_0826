/**
 * @file ws2812b.c
 * @brief WS2812B RGB LED 驱动实现
 *
 * 使用 esp_led_strip 组件（RMT 驱动），支持 WS2812 协议。
 */
#include "ws2812b.h"
#include "../framework/framework.h"

#include "led_strip.h"
#include "esp_log.h"

static const char *TAG = "ws2812b";

static struct ws2812b g_ws2812b;

/* ------------------------------------------------------------------ */
/* 设备操作函数                                                       */
/* ------------------------------------------------------------------ */

static int ws2812b_init(void *self)
{
    struct ws2812b *led = (struct ws2812b *)self;

    led->red   = 0;
    led->green = 0;
    led->blue  = 0;

    led_strip_config_t strip_config = {
        .strip_gpio_num           = WS2812B_GPIO,
        .max_leds                 = WS2812B_NUM_LEDS,
        .led_model                = LED_MODEL_WS2812,
        .color_component_format   = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = 10 * 1000 * 1000,  /* 10MHz */
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false,
        },
    };

    esp_err_t ret = led_strip_new_rmt_device(
        &strip_config, &rmt_config,
        (led_strip_handle_t *)&led->strip_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LED strip init failed: %s", esp_err_to_name(ret));
        return -1;
    }

    ESP_LOGI(TAG, "WS2812B 初始化完成，GPIO=%d，共 %d 颗",
             WS2812B_GPIO, WS2812B_NUM_LEDS);
    return 0;
}

static int ws2812b_read(void *self, void *buf, size_t len)
{
    struct ws2812b *led = (struct ws2812b *)self;

    if (len < 3)
        return -1;

    uint8_t *rgb = (uint8_t *)buf;
    rgb[0] = led->red;
    rgb[1] = led->green;
    rgb[2] = led->blue;
    return 3;
}

static int ws2812b_write(void *self, const void *buf, size_t len)
{
    struct ws2812b *led = (struct ws2812b *)self;

    if (len < 3)
        return -1;

    const uint8_t *rgb = (const uint8_t *)buf;
    led->red   = rgb[0];
    led->green = rgb[1];
    led->blue  = rgb[2];

    led_strip_handle_t handle = (led_strip_handle_t)led->strip_handle;

    esp_err_t ret = led_strip_set_pixel(handle, 0,
                                        led->red, led->green, led->blue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LED set pixel failed: %s", esp_err_to_name(ret));
        return -1;
    }

    ret = led_strip_refresh(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LED refresh failed: %s", esp_err_to_name(ret));
        return -1;
    }

    return 3;
}

static int ws2812b_deinit(void *self)
{
    struct ws2812b *led = (struct ws2812b *)self;

    if (led->strip_handle) {
        led_strip_del((led_strip_handle_t)led->strip_handle);
        led->strip_handle = NULL;
    }
    return 0;
}

static const struct DeviceOps ws2812b_ops = {
    .init   = ws2812b_init,
    .read   = ws2812b_read,
    .write  = ws2812b_write,
    .deinit = ws2812b_deinit,
};

static struct Device g_ws2812b_device = {
    .name = "ws2812b",
    .data = &g_ws2812b,
    .ops  = &ws2812b_ops,
};

struct Device *Ws2812b_get_device(void)
{
    return &g_ws2812b_device;
}
