/**
 * @file display.h
 * @brief LCD 显示驱动（ST7789 + LVGL）
 *
 * 初始化 ST7789 SPI 显示屏并注册到 LVGL。
 * 显示内容由 LVGL 管理，本驱动仅负责初始化。
 */
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include "esp_lcd_panel_ops.h"

struct Device;

/** 显示设备私有数据 */
struct Display {
    esp_lcd_panel_handle_t panel_handle;  ///< LCD 面板句柄
    int width;                            ///< 屏幕宽度（像素）
    int height;                           ///< 屏幕高度（像素）
};

/**
 * @brief 获取显示设备实例
 * @return 设备指针
 */
struct Device *Display_get_device(void);

#endif /* DISPLAY_H */
