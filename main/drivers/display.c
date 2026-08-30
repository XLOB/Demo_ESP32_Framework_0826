/**
 * @file display.c
 * @brief LCD 显示驱动实现（ST7789 + LVGL）
 *
 * 硬件配置（xhyOS / ESP32-S3）：
 *   - 控制器：ST7789
 *   - 分辨率：240x240
 *   - 接口：SPI (SPI3_HOST)
 *   - 引脚：MOSI=21, SCLK=17, CS=15, DC=14
 *   - 背光：GPIO16（由 backlight 驱动控制）
 *
 * 注意：显示内容完全由 LVGL 接管，本驱动仅负责初始化。
 */
#include "display.h"
#include "../framework/framework.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

static const char *TAG = "display";

/* ===== 硬件配置 ===== */

#define LCD_PIN_MOSI   21
#define LCD_PIN_SCLK   17
#define LCD_PIN_CS     15
#define LCD_PIN_DC     14
#define LCD_PIN_RST    -1   /* 无硬件复位脚 */

#define LCD_WIDTH      240
#define LCD_HEIGHT     240
#define LCD_SPI_HOST   SPI3_HOST

/* LVGL 缓冲配置 */
#define LVGL_BUF_LINES      20    /* 行缓冲行数 */
#define LVGL_TASK_PRIORITY  4
#define LVGL_TASK_STACK     4096

static struct Display g_display;

/* ------------------------------------------------------------------ */
/* 设备操作函数                                                       */
/* ------------------------------------------------------------------ */

static int display_init(void *self)
{
    struct Display *disp = (struct Display *)self;
    esp_err_t ret;
    esp_lcd_panel_io_handle_t io_handle = NULL;

    /* ===== 1. 初始化 SPI 总线 ===== */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = LCD_PIN_MOSI,
        .miso_io_num     = -1,
        .sclk_io_num     = LCD_PIN_SCLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * 2,
    };
    ret = spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI 总线初始化失败: %s", esp_err_to_name(ret));
        return -1;
    }

    /* ===== 2. 创建 LCD Panel IO ===== */
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num      = LCD_PIN_DC,
        .cs_gpio_num      = LCD_PIN_CS,
        .spi_mode        = 0,
        .pclk_hz         = 20 * 1000 * 1000,  /* 20 MHz */
        .trans_queue_depth = 10,
        .lcd_cmd_bits    = 8,
        .lcd_param_bits  = 8,
    };
    ret = esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)LCD_SPI_HOST,
        &io_config, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel IO 创建失败: %s", esp_err_to_name(ret));
        goto err_free_spi;
    }

    /* ===== 3. 创建 ST7789 面板 ===== */
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num   = LCD_PIN_RST,
        .bits_per_pixel   = 16,
        .rgb_ele_order    = LCD_RGB_ELEMENT_ORDER_BGR,
    };
    ret = esp_lcd_new_panel_st7789(io_handle, &panel_config, &disp->panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ST7789 面板创建失败: %s", esp_err_to_name(ret));
        goto err_del_io;
    }

    /* ===== 4. 初始化面板 ===== */
    ret = esp_lcd_panel_reset(disp->panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "面板复位失败: %s", esp_err_to_name(ret));
        goto err_del_panel;
    }
    ret = esp_lcd_panel_init(disp->panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "面板初始化失败: %s", esp_err_to_name(ret));
        goto err_del_panel;
    }
    ret = esp_lcd_panel_invert_color(disp->panel_handle, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "颜色反转设置失败: %s", esp_err_to_name(ret));
        goto err_del_panel;
    }
    ret = esp_lcd_panel_disp_on_off(disp->panel_handle, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "显示开启失败: %s", esp_err_to_name(ret));
        goto err_del_panel;
    }

    disp->width  = LCD_WIDTH;
    disp->height = LCD_HEIGHT;

    /* ===== 5. 初始化 LVGL 端口 ===== */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority      = LVGL_TASK_PRIORITY,
        .task_stack         = LVGL_TASK_STACK,
        .task_affinity      = -1,               /* 不固定核心 */
        .task_max_sleep_ms  = 500,
        .timer_period_ms    = 5,
    };
    ret = lvgl_port_init(&lvgl_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LVGL 端口初始化失败: %s", esp_err_to_name(ret));
        goto err_del_panel;
    }

    /* ===== 6. 将 LCD 面板注册到 LVGL ===== */
    /*
     * 关键参数说明：
     * - swap_bytes=true: SPI 传输 16 位像素时，ESP32 内存是小端序，
     *   但 ST7789 期望大端序。不交换字节会导致抗锯齿像素错乱，
     *   文字边缘出现"豹纹"斑点。
     * - BGR 颜色顺序由面板 MADCTL 寄存器（rgb_ele_order）控制，
     *   与 swap_bytes 是独立的两件事。
     */
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle    = io_handle,
        .panel_handle = disp->panel_handle,
        .buffer_size  = LCD_WIDTH * LVGL_BUF_LINES,
        .double_buffer = true,
        .hres         = LCD_WIDTH,
        .vres         = LCD_HEIGHT,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .monochrome   = false,
        .rotation = {
            .swap_xy  = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma    = true,    /* 使用 DMA 内存 */
            .buff_spiram = true,    /* 优先使用 PSRAM */
            .swap_bytes  = true,    /* RGB565 字节交换（SPI 显示必需） */
        },
    };
    lv_display_t *lv_disp = lvgl_port_add_disp(&disp_cfg);
    if (lv_disp == NULL) {
        ESP_LOGE(TAG, "LVGL 显示注册失败");
        lvgl_port_deinit();
        goto err_del_panel;
    }

    ESP_LOGI(TAG, "显示初始化完成，%dx%d，LVGL 已接管",
             disp->width, disp->height);
    return 0;

err_del_panel:
    esp_lcd_panel_del(disp->panel_handle);
    disp->panel_handle = NULL;
err_del_io:
    esp_lcd_panel_io_del(io_handle);
err_free_spi:
    spi_bus_free(LCD_SPI_HOST);
    return -1;
}

static int display_write(void *self, const void *buf, size_t len)
{
    (void)self; (void)buf; (void)len;
    return 0; /* LVGL 接管显示，写入操作空实现 */
}

static int display_read(void *self, void *buf, size_t len)
{
    (void)self; (void)buf; (void)len;
    return -1; /* 不支持读 */
}

static int display_deinit(void *self)
{
    struct Display *disp = (struct Display *)self;

    if (disp->panel_handle) {
        esp_lcd_panel_del(disp->panel_handle);
        disp->panel_handle = NULL;
    }
    return 0;
}

static const struct DeviceOps display_ops = {
    .init   = display_init,
    .read   = display_read,
    .write  = display_write,
    .deinit = display_deinit,
};

static struct Device g_display_device = {
    .name = "display",
    .data = &g_display,
    .ops  = &display_ops,
};

struct Device *Display_get_device(void)
{
    return &g_display_device;
}
