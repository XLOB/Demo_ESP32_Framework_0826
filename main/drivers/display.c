#include "display.h"
#include "../framework/framework.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>

static const char *TAG = "display";

// AI-VOX3 屏幕引脚
#define PIN_LCD_MOSI 21
#define PIN_LCD_SCLK 17
#define PIN_LCD_CS 15
#define PIN_LCD_DC 14
#define PIN_LCD_RST -1
#define PIN_LCD_BL 16

#define LCD_WIDTH 240
#define LCD_HEIGHT 240

static struct Display g_display;

// RGB565 颜色宏 (5位红 + 6位绿 + 5位蓝)
#define RGB565(r, g, b)                               \
    (((uint16_t)(((uint8_t)(r) >> 3) & 0x1F) << 11) | \
     ((uint16_t)(((uint8_t)(g) >> 2) & 0x3F) << 5) |  \
     ((uint16_t)(((uint8_t)(b) >> 3) & 0x1F)))

static int display_init(void *self)
{
    struct Display *disp = (struct Display *)self;

    // 1. 打开背光
    gpio_config_t bl_conf = {
        .pin_bit_mask = 1ULL << PIN_LCD_BL,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&bl_conf);
    gpio_set_level(PIN_LCD_BL, 1);

    // 2. 初始化 SPI 总线
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    // 3. 创建 LCD 面板 IO
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_LCD_DC,
        .cs_gpio_num = PIN_LCD_CS,
        .spi_mode = 0,
        .pclk_hz = 20 * 1000 * 1000, // 20MHz，稳定
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)SPI3_HOST,
        &io_config, &io_handle));

    // 4. 创建 ST7789 面板
    //    注意: AI-VOX3 的 ST7789 面板物理排列是 RGB
    //    esp_lcd_new_panel_st7789 内部会根据 rgb_ele_order 自动设置 MADCTL 的 BGR 位
    //    设 RGB = 不翻转颜色通道，设 BGR = 翻转红绿蓝
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RST,
        .bits_per_pixel = 16,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(
        io_handle, &panel_config, &disp->panel_handle));

    // 5. 初始化面板
    ESP_ERROR_CHECK(esp_lcd_panel_reset(disp->panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(disp->panel_handle));

    // 开启颜色反转 — ST7789 默认极性与 AI-VOX3 面板不匹配，需要反色
    // 不加这行会导致黑底白字变成白底黑字
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(disp->panel_handle, true));

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(disp->panel_handle, true));

    disp->width = LCD_WIDTH;
    disp->height = LCD_HEIGHT;

    ESP_LOGI(TAG, "屏幕初始化完成 (ST7789, RGB565, 240x240)");

    return 0;
}

// 内部填充矩形函数：所有写屏操作最终都调用它
// 使用 DMA 内存分配行缓冲，逐行推送到 LCD
static void display_fill_rect_internal(struct Display *disp,
                                       int x, int y, int w, int h,
                                       uint16_t color)
{
    color = (color & 0xF800) | ((color & 0x001F) << 5) | ((color & 0x07E0) >> 5); // 颜色通道翻转，ST7789 内部是 BGR 排列，而我们使用的是 RGB565

    // 参数裁剪
    if (x < 0)
    {
        w += x;
        x = 0;
    }
    if (y < 0)
    {
        h += y;
        y = 0;
    }
    if (x + w > disp->width)
        w = disp->width - x;
    if (y + h > disp->height)
        h = disp->height - y;
    if (w <= 0 || h <= 0)
        return;

    // 分配 DMA 可用的行缓冲
    uint16_t *line = heap_caps_malloc(w * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!line)
    {
        ESP_LOGE(TAG, "DMA 内存分配失败");
        return;
    }

    // 填充行缓冲
    for (int i = 0; i < w; i++)
    {
        line[i] = color;
    }

    // 逐行绘制
    for (int row = y; row < y + h; row++)
    {
        esp_lcd_panel_draw_bitmap(disp->panel_handle,
                                  x, row,
                                  x + w, row + 1,
                                  line);
    }

    free(line);
}

// 绘制完整帧缓冲 (RGB565)
static void display_draw_bitmap_internal(struct Display *disp,
                                         int x, int y, int w, int h,
                                         const uint16_t *bitmap)
{
    esp_lcd_panel_draw_bitmap(disp->panel_handle, x, y, x + w, y + h, bitmap);
}

static int display_write(void *self, const void *buf, size_t len)
{
    struct Display *disp = (struct Display *)self;

    // 如果传入了帧缓冲数据，直接绘制
    if (buf && len >= (size_t)LCD_WIDTH * LCD_HEIGHT * 2)
    {
        display_draw_bitmap_internal(disp, 0, 0, LCD_WIDTH, LCD_HEIGHT,
                                     (const uint16_t *)buf);
        return 0;
    }

    // 没有传入数据时：显示测试画面 (红蓝绿三色条)
    display_fill_rect_internal(disp, 0, 0, 80, LCD_HEIGHT, 0xF800);   // 红
    display_fill_rect_internal(disp, 80, 0, 80, LCD_HEIGHT, 0x07E0);  // 绿
    display_fill_rect_internal(disp, 160, 0, 80, LCD_HEIGHT, 0x001F); // 蓝

    return 0;
}

static int display_read(void *self, void *buf, size_t len)
{
    return -1; // 屏幕不支持读
}

static int display_deinit(void *self)
{
    struct Display *disp = (struct Display *)self;
    if (disp->panel_handle)
    {
        esp_lcd_panel_del(disp->panel_handle);
        disp->panel_handle = NULL;
    }
    return 0;
}

static const struct DeviceOps display_ops = {
    .init = display_init,
    .read = display_read,
    .write = display_write,
    .deinit = display_deinit,
};

static struct Device g_display_device = {
    .name = "display",
    .data = &g_display,
    .ops = &display_ops,
};

struct Device *Display_get_device(void)
{
    return &g_display_device;
}

int display_clear(struct Device *dev, uint16_t color)
{
    if (!dev || !dev->data)
        return -1;
    struct Display *disp = (struct Display *)dev->data;

    display_fill_rect_internal(disp, 0, 0, disp->width, disp->height, color);
    return 0;
}

int display_fill_rect(struct Device *dev, int x, int y, int w, int h, uint16_t color)
{
    if (!dev || !dev->data)
        return -1;
    struct Display *disp = (struct Display *)dev->data;

    display_fill_rect_internal(disp, x, y, w, h, color);
    return 0;
}

static inline uint16_t display_swap_green_blue(uint16_t color)
{
    return (color & 0xF800) | ((color & 0x001F) << 5) | ((color & 0x07E0) >> 5);
}
