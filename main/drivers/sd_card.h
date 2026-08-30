/**
 * @file sd_card.h
 * @brief SD 卡驱动（SDMMC 1-bit 模式）
 *
 * xhyOS SD 卡引脚定义（来自官方文档）：
 *   CLK  = GPIO39
 *   CMD  = GPIO38
 *   DAT0 = GPIO40
 *   模式: SD_MMC 1-bit
 */
#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sdmmc_cmd.h"

struct Device;

/* ===== 配置 ===== */

#define SD_MOUNT_POINT  "/sdcard"   ///< 挂载路径

/* 引脚定义（xhyOS 官方配置） */
#define SD_PIN_CLK      39   ///< CLK
#define SD_PIN_CMD      38   ///< CMD
#define SD_PIN_D0       40   ///< DAT0
#define SD_PIN_D1       -1   ///< 未连接
#define SD_PIN_D2       -1   ///< 未连接
#define SD_PIN_D3       -1   ///< 未连接

#define SD_BUS_WIDTH    1    ///< 总线宽度（1-bit）

/**
 * @brief SD 卡设备私有数据
 */
struct SdCard {
    bool            mounted;       ///< 是否已挂载
    sdmmc_card_t   *card;          ///< SD 卡句柄
    uint64_t        total_bytes;   ///< 总容量（字节）
    uint64_t        free_bytes;    ///< 剩余容量（字节）
    char            name[32];      ///< 卡名称
};

/** 获取 SD 卡设备实例 */
struct Device *SdCard_get_device(void);

/**
 * @brief 读取文件全部内容
 * @param path     文件路径（相对于挂载点，如 "/hello.txt"）
 * @param buf      接收缓冲区
 * @param max_len  缓冲区最大长度
 * @return 实际读取字节数，-1 失败
 */
int sd_card_read_file(const char *path, char *buf, size_t max_len);

/**
 * @brief 写入文件（覆盖）
 * @param path  文件路径
 * @param buf   数据缓冲区
 * @param len   数据长度
 * @return 实际写入字节数，-1 失败
 */
int sd_card_write_file(const char *path, const char *buf, size_t len);

/**
 * @brief 追加写入文件
 * @param path  文件路径
 * @param buf   数据缓冲区
 * @param len   数据长度
 * @return 实际写入字节数，-1 失败
 */
int sd_card_append_file(const char *path, const char *buf, size_t len);

/**
 * @brief 列出目录内容（通过日志输出）
 * @param dir_path  目录路径（相对于挂载点，"/" 表示根目录）
 * @return 文件数量，-1 失败
 */
int sd_card_list_files(const char *dir_path);

/**
 * @brief 检查文件是否存在
 * @param path  文件路径
 * @return 1 存在，0 不存在，-1 错误
 */
int sd_card_file_exists(const char *path);

#endif /* SD_CARD_H */
