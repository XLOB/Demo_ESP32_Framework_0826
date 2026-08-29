#ifndef BATTERY_H
#define BATTERY_H

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

struct Device;

struct Battery
{
    int voltage_mv; // 电压，单位毫伏
    int percent;    // 电量百分比，粗略估算

    // ADC 句柄和校准句柄
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t cali_handle;

    // 可配置参数
    int voltage_divider_ratio; // 分压比，例如2表示实际电压 = ADC读数 * 2
};

struct Device *Battery_get_device(void);

#endif