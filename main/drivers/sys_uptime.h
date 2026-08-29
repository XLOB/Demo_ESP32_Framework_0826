/**
 * @file sys_uptime.h
 * @brief 系统运行时间驱动
 *
 * 基于 esp_timer 高精度计时器提供系统运行时间（秒）。
 */
#ifndef SYS_UPTIME_H
#define SYS_UPTIME_H

#include <stdint.h>

struct Device;

/** 系统运行时间设备私有数据 */
struct SysUptime {
    uint32_t seconds;  ///< 系统运行时间（秒）
};

/**
 * @brief 获取系统运行时间设备实例
 * @return 设备指针
 */
struct Device *SysUptime_get_device(void);

#endif /* SYS_UPTIME_H */
