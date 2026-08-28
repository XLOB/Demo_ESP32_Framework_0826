#ifndef SYS_UPTIME_H
#define SYS_UPTIME_H

#include <stdint.h>

struct Device;

struct SysUptime
{
    uint32_t seconds;
};

struct Device *SysUptime_get_device(void);

#endif