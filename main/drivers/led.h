#ifndef LED_H
#define LED_H

struct Device;

struct Led
{
    int state; // 0=关，1=开
};

struct Device *Led_get_device(void);

#endif