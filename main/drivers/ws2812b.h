#ifndef WS2812B_H
#define WS2812B_H

#include <stdint.h>
#include "led_strip.h"

struct Device;

struct ws2812b
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    led_strip_handle_t strip_handle; // LED 灯条句柄
};

struct Device *Ws2812b_get_device(void);

#endif
