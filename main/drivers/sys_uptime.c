#include "sys_uptime.h"
#include "../framework/framework.h"
#include "esp_timer.h"
#include "esp_log.h"

static struct SysUptime g_uptime;

static int sys_uptime_init(void *self)
{
    struct SysUptime *uptime = (struct SysUptime *)self;
    uptime->seconds = 0;
    return 0;
}

static int sys_uptime_read(void *self, void *buf, size_t len)
{
    struct SysUptime *uptime = (struct SysUptime *)self;

    if (len < sizeof(uint32_t))
        return -1;

    // esp_timer_get_time() 返回微秒
    uint32_t seconds = (uint32_t)(esp_timer_get_time() / 1000000ULL);

    uptime->seconds = seconds;

    memcpy(buf, &uptime->seconds, sizeof(uint32_t));

    return sizeof(uint32_t);
}

static int sys_uptime_write(void *self, const void *buf, size_t len)
{
    return -1; // 运行时间不支持写
}

static int sys_uptime_deinit(void *self)
{
    return 0;
}

static const struct DeviceOps sys_uptime_ops = {
    .init = sys_uptime_init,
    .read = sys_uptime_read,
    .write = sys_uptime_write,
    .deinit = sys_uptime_deinit,
};

static struct Device g_sys_uptime_device = {
    .name = "sys_uptime",
    .data = &g_uptime,
    .ops = &sys_uptime_ops,
};

struct Device *SysUptime_get_device(void)
{
    return &g_sys_uptime_device;
}