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
// 注意：背光由 backlight 驱动控制，此处不再定义 PIN_LCD_BL

#define LCD_WIDTH 240
#define LCD_HEIGHT 240

static struct Display g_display;

// RGB565 颜色宏 (5位红 + 6位绿 + 5位蓝)
#define RGB565(r, g, b)                               \
    (((uint16_t)(((uint8_t)(r) >> 3) & 0x1F) << 11) | \
     ((uint16_t)(((uint8_t)(g) >> 2) & 0x3F) << 5) |  \
     ((uint16_t)(((uint8_t)(b) >> 3) & 0x1F)))

// 颜色通道翻转：标准 RGB565 转面板需要的 BGR565
static inline uint16_t display_swap_green_blue(uint16_t color)
{
    return (color & 0xF800) | ((color & 0x001F) << 5) | ((color & 0x07E0) >> 5);
}

static int display_init(void *self)
{
    struct Display *disp = (struct Display *)self;

    // 1. 初始化 SPI 总线
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    // 2. 创建 LCD 面板 IO
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_LCD_DC,
        .cs_gpio_num = PIN_LCD_CS,
        .spi_mode = 0,
        .pclk_hz = 20 * 1000 * 1000, // 20MHz
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)SPI3_HOST,
        &io_config, &io_handle));

    // 3. 创建 ST7789 面板
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RST,
        .bits_per_pixel = 16,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(
        io_handle, &panel_config, &disp->panel_handle));

    // 4. 初始化面板
    ESP_ERROR_CHECK(esp_lcd_panel_reset(disp->panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(disp->panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(disp->panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(disp->panel_handle, true));

    // 5. 预分配行缓冲
    disp->width = LCD_WIDTH;
    disp->height = LCD_HEIGHT;
    disp->line_buffer = heap_caps_malloc(disp->width * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (disp->line_buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate line buffer");
        return -1;
    }

    ESP_LOGI(TAG, "屏幕初始化完成 (ST7789, RGB565, 240x240)");
    return 0;
}

// 内部填充矩形：统一处理颜色交换和裁剪
static void display_fill_rect_internal(struct Display *disp,
                                       int x, int y, int w, int h,
                                       uint16_t color)
{
    // 统一颜色转换
    color = display_swap_green_blue(color);

    // 边界裁剪
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

    // 填充行缓冲
    for (int i = 0; i < w; i++)
    {
        disp->line_buffer[i] = color;
    }

    // 设置窗口并逐行绘制
    for (int row = y; row < y + h; row++)
    {
        esp_lcd_panel_draw_bitmap(disp->panel_handle,
                                  x, row,
                                  x + w, row + 1,
                                  disp->line_buffer);
    }
}

// 内部绘制整帧图像（假设颜色已处理）
static void display_draw_bitmap_internal(struct Display *disp,
                                         int x, int y, int w, int h,
                                         const uint16_t *bitmap)
{
    esp_lcd_panel_draw_bitmap(disp->panel_handle, x, y, x + w, y + h, bitmap);
}

// 显示简单的 xhyOS Logo（白色 X）
static void display_show_logo_internal(struct Display *disp)
{
    // 清屏黑色
    display_fill_rect_internal(disp, 0, 0, disp->width, disp->height, 0x0000);

    // 画一个大的 X：两条对角线
    uint16_t white = 0xFFFF;
    int step = 10; // 每个方块的边长

    for (int i = 0; i < disp->width; i += step)
    {
        // 左上到右下
        display_fill_rect_internal(disp, i, i, step, step, white);
        // 右上到左下
        display_fill_rect_internal(disp, disp->width - i - step, i, step, step, white);
    }

    // 中心画一个小方块
    int center = disp->width / 2;
    display_fill_rect_internal(disp, center - 20, center - 20, 40, 40, white);
}

static int display_write(void *self, const void *buf, size_t len)
{
    struct Display *disp = (struct Display *)self;

    // 如果传入完整帧缓冲，直接绘制
    if (buf && len >= (size_t)LCD_WIDTH * LCD_HEIGHT * 2)
    {
        display_draw_bitmap_internal(disp, 0, 0, LCD_WIDTH, LCD_HEIGHT,
                                     (const uint16_t *)buf);
        return 0;
    }

    // 否则显示 Logo
    display_show_logo_internal(disp);
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
    if (disp->line_buffer)
    {
        free(disp->line_buffer);
        disp->line_buffer = NULL;
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