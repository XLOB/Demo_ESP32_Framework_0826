#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "sdmmc_cmd.h"

struct Device;

// SD 卡挂载点（VFS 路径前缀）
#define SD_MOUNT_POINT "/sdcard"

// AI-VOX3 SD 卡引脚定义（来自官方文档）
// 接口模式: SD_MMC 1-bit
//   CLK  = GPIO39
//   CMD  = GPIO38
//   DAT0 = GPIO40
#define SD_PIN_CLK  39
#define SD_PIN_CMD  38
#define SD_PIN_D0   40
#define SD_PIN_D1   -1   // 未连接，1-bit 模式
#define SD_PIN_D2   -1   // 未连接
#define SD_PIN_D3   -1   // 未连接

// SD 卡总线宽度
#define SD_BUS_WIDTH 1   // 1-bit 模式

/**
 * @brief SD 卡设备私有数据
 *
 * 存储卡的状态信息和句柄。
 */
struct SdCard
{
    bool mounted;           // 是否已挂载
    sdmmc_card_t *card;     // SD 卡句柄（由 esp_vfs_fat_sdmmc_mount 填充）
    uint64_t total_bytes;   // 总容量（字节）
    uint64_t free_bytes;    // 剩余容量（字节）
    char name[32];          // 卡名称
};

/**
 * @brief 获取 SD 卡设备实例
 * @return 指向 SD 卡设备的指针
 */
struct Device *SdCard_get_device(void);

/**
 * @brief 读取 SD 卡上指定文件的全部内容
 * @param path 文件路径（相对于挂载点，如 "/hello.txt"）
 * @param buf  接收数据的缓冲区
 * @param max_len 缓冲区最大长度
 * @return 实际读取的字节数，-1 表示失败
 */
int sd_card_read_file(const char *path, char *buf, size_t max_len);

/**
 * @brief 向 SD 卡写入文件（覆盖写入）
 * @param path 文件路径（相对于挂载点）
 * @param buf  数据缓冲区
 * @param len  数据长度
 * @return 实际写入的字节数，-1 表示失败
 */
int sd_card_write_file(const char *path, const char *buf, size_t len);

/**
 * @brief 追加数据到 SD 卡文件末尾
 * @param path 文件路径
 * @param buf  数据缓冲区
 * @param len  数据长度
 * @return 实际写入的字节数，-1 表示失败
 */
int sd_card_append_file(const char *path, const char *buf, size_t len);

/**
 * @brief 列出指定目录下的文件（通过日志输出）
 * @param dir_path 目录路径（相对于挂载点，如 "/" 表示根目录）
 * @return 文件数量，-1 表示失败
 */
int sd_card_list_files(const char *dir_path);

/**
 * @brief 检查文件是否存在
 * @param path 文件路径
 * @return 1 存在，0 不存在，-1 错误
 */
int sd_card_file_exists(const char *path);

#endif // SD_CARD_H
