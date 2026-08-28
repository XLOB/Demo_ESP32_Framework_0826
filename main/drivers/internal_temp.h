// internal_temp.h
#ifndef INTERNAL_TEMP_H
#define INTERNAL_TEMP_H

struct Device;

struct InternalTemp
{
    float temperature;
};

struct Device *InternalTemp_get_device(void);

#endif // INTERNAL_TEMP_H