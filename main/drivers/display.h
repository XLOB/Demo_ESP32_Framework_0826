#ifndef DISPLAY_H
#define DISPLAY_H

#include <stddef.h>
#include "esp_lcd_panel_ops.h"

struct Device;

struct Display
{
    esp_lcd_panel_handle_t panel_handle;
    int width;
    int height;
};

struct Device *Display_get_device(void);

int display_clear(struct Device *dev, uint16_t color);
int display_fill_rect(struct Device *dev, int x, int y, int w, int h, uint16_t color);

#endif