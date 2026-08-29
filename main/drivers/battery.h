/**
 * @file battery.h
 * @brief 电池驱动（ADC 电压采样 + 分压比换算）
 *
 * 通过 ADC 采样电池电压（经电阻分压），换算为实际电压和电量百分比。
 */
#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>

struct Device;

/** 电池设备私有数据 */
struct Battery {
    int  voltage_mv;            ///< 电池电压（毫伏）
    int  percent;               ///< 电量百分比（0~100，粗略估算）
    int  voltage_divider_ratio; ///< 分压比（例如 2 表示实际电压 = ADC 读数 × 2）

    /* 内部句柄（声明为 void* 以减少头文件依赖） */
    void *adc_handle;           ///< ADC oneshot 句柄
    void *cali_handle;          ///< ADC 校准句柄
};

/**
 * @brief 获取电池设备实例
 * @return 设备指针
 */
struct Device *Battery_get_device(void);

#endif /* BATTERY_H */
