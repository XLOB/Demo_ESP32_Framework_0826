/**
 * @file sensor.h
 * @brief 虚拟传感器驱动（调试用）
 *
 * 一个简单的虚拟传感器，用于测试框架和应用逻辑。
 * 可通过 write 设置值，read 读取值。
 */
#ifndef SENSOR_H
#define SENSOR_H

struct Device;

/** 虚拟传感器私有数据 */
struct VirtualSensor {
    int value;  ///< 传感器数值
};

/**
 * @brief 获取虚拟传感器设备实例
 * @return 设备指针
 */
struct Device *VirtualSensor_get_device(void);

#endif /* SENSOR_H */
