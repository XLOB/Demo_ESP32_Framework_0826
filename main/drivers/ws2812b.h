#ifndef WS2812B_H
#define WS2812B_H

#include <stdint.h>

struct Device;

struct ws2812b
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

struct Device *Ws2812b_get_device(void);

#endif