#ifndef UART_PH2_H
#define UART_PH2_H

#include <stddef.h>

struct Device;

struct UartPh2
{
    int uart_num;
    int baud_rate;
};

struct Device *UartPh2_get_device(void);

#endif