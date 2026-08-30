/**
 * @file lua_runtime.h
 * @brief Lua 运行时组件
 *
 * 管理 Lua VM 的创建/销毁，提供从文件运行脚本的接口。
 * 所有系统模块（device、sys 等）在此注册并暴露给 Lua。
 */
#ifndef LUA_RUNTIME_H
#define LUA_RUNTIME_H

#include <stddef.h>

/* 前向声明，避免引入 lua.h */
struct lua_State;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 Lua 运行时
 *
 * 创建 Lua VM，注册所有系统模块（device、sys 等）。
 *
 * @return 0 成功，-1 失败
 */
int lua_runtime_init(void);

/**
 * @brief 运行指定路径的 Lua 脚本文件
 *
 * 从文件系统（SD 卡）读取 .lua 文件并执行。
 * 脚本执行完后返回，VM 保持运行状态（可继续执行其他脚本）。
 *
 * @param path 脚本文件绝对路径（如 "/sdcard/apps/hello.lua"）
 * @return 0 成功；-1 失败（文件不存在/语法错误/运行时错误）
 */
int lua_run_file(const char *path);

/**
 * @brief 运行一段 Lua 代码字符串
 *
 * @param code Lua 代码字符串
 * @return 0 成功；-1 失败
 */
int lua_run_string(const char *code);

/**
 * @brief 销毁 Lua 运行时
 *
 * 关闭 VM，释放所有资源。
 */
void lua_runtime_deinit(void);

/**
 * @brief 获取 Lua VM 句柄
 *
 * 供其他组件注册 Lua 模块时使用。
 *
 * @return Lua state 指针，未初始化返回 NULL
 */
struct lua_State *lua_runtime_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* LUA_RUNTIME_H */
