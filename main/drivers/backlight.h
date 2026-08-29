#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include <stdint.h>

struct Device;

struct Backlight
{
    uint8_t brightness; // 0~100
};

struct Device *Backlight_get_device(void);

#endif