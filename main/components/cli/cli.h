/**
 * @file cli.h
 * @brief 命令行接口（CLI）组件
 *
 * 提供交互式命令行界面，支持从 UART 输入命令并执行。
 * 内置基础命令（help、ls、run、info 等），并可扩展注册新命令。
 *
 * 设计为输入输出可替换：当前使用 UART，后续可替换为屏幕终端。
 */
#ifndef CLI_H
#define CLI_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 命令处理函数原型
 *  @param argc 参数个数（包含命令名）
 *  @param argv 参数数组
 *  @return 0 成功，-1 失败
 */
typedef int (*cli_cmd_handler_t)(int argc, char *argv[]);

/**
 * @brief 初始化并启动 CLI
 *
 * 创建 CLI 任务，开始从 UART 读取输入并处理命令。
 *
 * @return 0 成功，-1 失败
 */
int cli_init(void);

/**
 * @brief 注册一条命令
 *
 * @param name     命令名（唯一）
 * @param handler  命令处理函数
 * @param help     帮助说明文字
 * @return 0 成功，-1 失败（命令已存在或表满）
 */
int cli_register_cmd(const char *name, cli_cmd_handler_t handler, const char *help);

/**
 * @brief 执行一行命令字符串
 *
 * 对外暴露，方便其他组件调用（比如脚本执行器、屏幕终端等）。
 *
 * @param line  命令行字符串（会被修改，按空格分割）
 * @return 命令执行结果
 */
int cli_exec_line(char *line);

#ifdef __cplusplus
}
#endif

#endif /* CLI_H */
