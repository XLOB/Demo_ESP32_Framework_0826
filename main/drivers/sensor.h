#ifndef SENSOR_H
#define SENSOR_H

struct Device;

struct VirtualSensor
{
    int value;
};

struct Device *VirtualSensor_get_device(void);

#endif