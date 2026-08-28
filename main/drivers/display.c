#include "display.h"
#include "../framework/framework.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "esp_log.h"

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
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST, &io_config, &io_handle));

    // 4. 创建 ST7789 面板
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RST,
        .bits_per_pixel = 16,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &disp->panel_handle));

    // 5. 初始化面板
    ESP_ERROR_CHECK(esp_lcd_panel_reset(disp->panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(disp->panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(disp->panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(disp->panel_handle, true));

    disp->width = LCD_WIDTH;
    disp->height = LCD_HEIGHT;

    ESP_LOGI(TAG, "屏幕初始化完成");

    return 0;
}

static int display_write(void *self, const void *buf, size_t len)
{
    struct Display *disp = (struct Display *)self;

    // 暂时简单清屏为红色，忽略 buf 和 len
    uint16_t red[LCD_WIDTH];
    for (int i = 0; i < LCD_WIDTH; i++)
    {
        red[i] = 0xF800;
    }
    for (int y = 0; y < LCD_HEIGHT; y++)
    {
        esp_lcd_panel_draw_bitmap(disp->panel_handle, 0, y, LCD_WIDTH, y + 1, red);
    }

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