/**
 * @file ws2812b.h
 * @brief WS2812B RGB LED 驱动
 *
 * 使用 RMT 外设驱动 WS2812B 地址式 RGB LED。
 * 颜色格式：GRB（WS2812 标准顺序）。
 */
#ifndef WS2812B_H
#define WS2812B_H

#include <stdint.h>

struct Device;

/* ===== 硬件配置 ===== */

#define WS2812B_GPIO     41   ///< 数据引脚
#define WS2812B_NUM_LEDS 1    ///< LED 数量

/** WS2812B 设备私有数据 */
struct ws2812b {
    uint8_t red;              ///< 红色分量（0~255）
    uint8_t green;            ///< 绿色分量（0~255）
    uint8_t blue;             ///< 蓝色分量（0~255）
    void   *strip_handle;     ///< LED strip 句柄（led_strip_handle_t）
};

/**
 * @brief 获取 WS2812B 设备实例
 * @return 设备指针
 */
struct Device *Ws2812b_get_device(void);

#endif /* WS2812B_H */
