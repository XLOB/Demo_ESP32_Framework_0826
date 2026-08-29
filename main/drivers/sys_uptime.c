/**
 * @file sys_uptime.c
 * @brief 系统运行时间驱动实现
 */
#include "sys_uptime.h"
#include "../framework/framework.h"

#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "sys_uptime";

static struct SysUptime g_uptime;

/* ------------------------------------------------------------------ */
/* 设备操作函数                                                       */
/* ------------------------------------------------------------------ */

static int sys_uptime_init(void *self)
{
    struct SysUptime *uptime = (struct SysUptime *)self;
    uptime->seconds = 0;
    ESP_LOGI(TAG, "系统运行时间统计初始化完成");
    return 0;
}

static int sys_uptime_read(void *self, void *buf, size_t len)
{
    struct SysUptime *uptime = (struct SysUptime *)self;

    if (len < sizeof(uint32_t))
        return -1;

    /* esp_timer_get_time() 返回微秒级高精度时间 */
    uint32_t seconds = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    uptime->seconds = seconds;

    memcpy(buf, &uptime->seconds, sizeof(uint32_t));
    return sizeof(uint32_t);
}

static int sys_uptime_write(void *self, const void *buf, size_t len)
{
    (void)self; (void)buf; (void)len;
    return -1; /* 只读设备 */
}

static int sys_uptime_deinit(void *self)
{
    (void)self;
    return 0;
}

static const struct DeviceOps sys_uptime_ops = {
    .init   = sys_uptime_init,
    .read   = sys_uptime_read,
    .write  = sys_uptime_write,
    .deinit = sys_uptime_deinit,
};

static struct Device g_sys_uptime_device = {
    .name = "sys_uptime",
    .data = &g_uptime,
    .ops  = &sys_uptime_ops,
};

struct Device *SysUptime_get_device(void)
{
    return &g_sys_uptime_device;
}
