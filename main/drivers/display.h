#ifndef DISPLAY_H
#define DISPLAY_H

#include <stddef.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

struct Device;

struct Display
{
    esp_lcd_panel_handle_t panel_handle; // 改成正确类型
    int width;
    int height;
};

struct Device *Display_get_device(void);

#endif