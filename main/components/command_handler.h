/**
 * @file command_handler.h
 * @brief 命令处理器
 *
 * 将按键事件映射为命令 ID，并支持为每个命令注册多个回调函数。
 * 内部创建独立任务消费按键队列，按顺序执行所有已注册的回调。
 *
 * 使用方式：
 *   1. command_handler_init() 初始化（自动注册默认命令）
 *   2. command_handler_register() 注册自定义命令回调
 *   3. 按键事件会自动触发对应命令的所有回调
 */
#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <stdint.h>

/** 命令回调函数类型 */
typedef void (*cmd_callback_t)(void *arg);

/* ===== 预定义命令 ID ===== */

#define CMD_KEY_A_SHORT   1  ///< A 键短按
#define CMD_KEY_B_SHORT   2  ///< B 键短按
/* 预留：长按、双击等可后续扩展 */

/**
 * @brief 初始化命令处理器
 *
 * 创建内部任务消费按键队列，并注册默认命令：
 *   - CMD_KEY_A_SHORT → 切换背光亮度
 *   - CMD_KEY_B_SHORT → 打印传感器信息
 *
 * @return 0 成功，-1 失败
 */
int command_handler_init(void);

/**
 * @brief 注册命令回调
 *
 * 同一命令 ID 可注册多个回调，触发时按注册顺序依次执行。
 *
 * @param cmd_id  命令 ID
 * @param cb      回调函数指针
 * @param arg     传递给回调的用户参数（可为 NULL）
 * @return 0 成功，-1 失败（参数无效或表已满）
 */
int command_handler_register(uint32_t cmd_id, cmd_callback_t cb, void *arg);

/**
 * @brief 注销指定命令的某个回调
 * @param cmd_id  命令 ID
 * @param cb      要注销的回调函数指针
 * @return 0 成功，-1 失败（未找到）
 */
int command_handler_unregister(uint32_t cmd_id, cmd_callback_t cb);

#endif /* COMMAND_HANDLER_H */
