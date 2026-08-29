#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <stdint.h>

// 预定义命令 ID
#define CMD_KEY_A_SHORT 1 // A 键短按
#define CMD_KEY_B_SHORT 2 // B 键短按
// 以后可扩展长按、组合等

// 命令回调函数类型
typedef void (*cmd_callback_t)(void *arg);

/**
 * @brief 初始化命令处理器
 *
 * 创建内部任务，注册默认命令（可覆盖）
 * @return 0 成功，-1 失败
 */
int command_handler_init(void);

/**
 * @brief 注册自定义命令
 *
 * 若命令 ID 已存在，则覆盖旧回调
 * @param cmd_id  命令 ID
 * @param cb      回调函数
 * @param arg     传递给回调的参数（可为 NULL）
 * @return 0 成功，-1 失败
 */
int command_handler_register(uint32_t cmd_id, cmd_callback_t cb, void *arg);

/**
 * @brief 注销命令
 * @param cmd_id 命令 ID
 * @return 0 成功，-1 失败
 */
int command_handler_unregister(uint32_t cmd_id);

#endif