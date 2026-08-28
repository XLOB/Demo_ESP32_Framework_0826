#ifndef BATTERY_H
#define BATTERY_H

struct Device;

struct Battery
{
    int voltage_mv; // 电压，单位毫伏
    int percent;    // 电量百分比，粗略估算
};

struct Device *Battery_get_device(void);

#endif