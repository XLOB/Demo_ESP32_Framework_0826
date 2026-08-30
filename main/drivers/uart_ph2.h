/**
 * @file uart_ph2.h
 * @brief PH2.0 接口 UART 驱动
 *
 * 通过 UART 与外接模块通信，提供基础收发功能。
 */
#ifndef UART_PH2_H
#define UART_PH2_H

#include <stddef.h>
#include <stdint.h>

struct Device;

/* ===== 硬件配置 ===== */
/*
 * GPIO 分配表（避免冲突）：
 *   GPIO 14  = LCD DC
 *   GPIO 15  = LCD CS
 *   GPIO 16  = Backlight PWM
 *   GPIO 17  = LCD SPI SCLK  ← 原 UART TX，已移走
 *   GPIO 18  = UART RX
 *   GPIO 21  = LCD SPI MOSI
 *   GPIO 38  = SD CMD
 *   GPIO 39  = SD CLK
 *   GPIO 40  = SD DAT0
 *   GPIO 41  = WS2812B Data
 *   GPIO 45  = Key B
 *   GPIO 46  = Key A
 *
 *   GPIO 8   = UART TX（空闲引脚，已分配给 PH2.0 接口）
 */

#define UART_PH2_PORT_NUM   UART_NUM_1
#define UART_PH2_TX_PIN     8
#define UART_PH2_RX_PIN     18
#define UART_PH2_BAUD_RATE  115200
#define UART_PH2_BUF_SIZE   1024

/**
 * @brief 获取 PH2.0 UART 设备实例
 * @return 设备指针
 */
struct Device *UartPh2_get_device(void);

/**
 * @brief 通过 UART 发送数据
 * @param data 数据指针
 * @param len  数据长度
 * @return 实际发送的字节数，-1 表示失败
 */
int uart_ph2_send(const uint8_t *data, size_t len);

/**
 * @brief 从 UART 接收数据（非阻塞）
 * @param buf    接收缓冲区
 * @param len    缓冲区大小
 * @return 实际接收的字节数，-1 表示失败
 */
int uart_ph2_recv(uint8_t *buf, size_t len);

#endif /* UART_PH2_H */
