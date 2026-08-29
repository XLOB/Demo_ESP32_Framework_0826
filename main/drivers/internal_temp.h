/**
 * @file internal_temp.h
 * @brief 内部温度传感器驱动
 *
 * 使用 ESP32-S3 内置温度传感器，量程 -10°C ~ 80°C。
 */
#ifndef INTERNAL_TEMP_H
#define INTERNAL_TEMP_H

struct Device;

/** 内部温度设备私有数据 */
struct InternalTemp {
    float temperature;  ///< 当前温度（摄氏度）
};

/**
 * @brief 获取内部温度设备实例
 * @return 设备指针
 */
struct Device *InternalTemp_get_device(void);

#endif /* INTERNAL_TEMP_H */
