/**
 * @file sensor.c
 * @brief 虚拟传感器驱动实现（调试用）
 */
#include "sensor.h"
#include "../framework/framework.h"

static int sensor_init(void *self)
{
    struct VirtualSensor *sensor = (struct VirtualSensor *)self;
    sensor->value = 25; /* 初始值 25 */
    return 0;
}

static int sensor_read(void *self, void *buf, size_t len)
{
    struct VirtualSensor *sensor = (struct VirtualSensor *)self;
    if (len < sizeof(int))
        return -1;

    *(int *)buf = sensor->value;
    return sizeof(int);
}

static int sensor_write(void *self, const void *buf, size_t len)
{
    struct VirtualSensor *sensor = (struct VirtualSensor *)self;
    if (len < sizeof(int))
        return -1;

    sensor->value = *(const int *)buf;
    return sizeof(int);
}

static int sensor_deinit(void *self)
{
    (void)self;
    return 0;
}

static const struct DeviceOps sensor_ops = {
    .init   = sensor_init,
    .read   = sensor_read,
    .write  = sensor_write,
    .deinit = sensor_deinit,
};

static struct VirtualSensor g_sensor;
static struct Device g_sensor_device = {
    .name = "temp_sensor",
    .data = &g_sensor,
    .ops  = &sensor_ops,
};

struct Device *VirtualSensor_get_device(void)
{
    return &g_sensor_device;
}
