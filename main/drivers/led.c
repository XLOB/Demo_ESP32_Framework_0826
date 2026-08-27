#include "led.h"
#include "../framework/framework.h"

static int led_init(void *self)
{
    struct Led *led = (struct Led *)self;
    led->state = 0;
    return 0;
}

static int led_read(void *self, void *buf, size_t len)
{
    struct Led *led = (struct Led *)self;
    if (len < sizeof(int))
        return -1;
    *(int *)buf = led->state;
    return sizeof(int);
}

static int led_write(void *self, const void *buf, size_t len)
{
    struct Led *led = (struct Led *)self;
    if (len < sizeof(int))
        return -1;
    led->state = *(const int *)buf;
    return sizeof(int);
}

static int led_deinit(void *self)
{
    return 0;
}

static const struct DeviceOps led_ops = {
    .init = led_init,
    .read = led_read,
    .write = led_write,
    .deinit = led_deinit,
};

static struct Led g_led;
static struct Device g_led_device = {
    .name = "led",
    .data = &g_led,
    .ops = &led_ops,
};

struct Device *Led_get_device(void)
{
    return &g_led_device;
}