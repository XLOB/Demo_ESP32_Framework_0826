/**
 * @file lua_runtime.c
 * @brief Lua 运行时实现
 *
 * 封装 Lua VM 管理、脚本加载执行、系统模块注册。
 */
#include "lua_runtime.h"

#include <stdio.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "lua_runtime";

/* 全局 Lua VM 句柄 */
static lua_State *s_L = NULL;

/* ------------------------------------------------------------------ */
/* 公开的 VM 句柄获取（供内部绑定模块使用）                           */
/* ------------------------------------------------------------------ */

lua_State *lua_runtime_get_state(void)
{
    return s_L;
}

/* ------------------------------------------------------------------ */
/* 内部工具函数                                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief Lua panic 回调（VM 发生致命错误时调用）
 */
static int panic_cb(lua_State *L)
{
    const char *msg = lua_tostring(L, -1);
    ESP_LOGE(TAG, "Lua PANIC: %s", msg ? msg : "unknown");
    return 0;
}

/**
 * @brief 打印栈顶的错误信息
 */
static void report_error(lua_State *L, int status)
{
    if (status != LUA_OK) {
        const char *msg = lua_tostring(L, -1);
        ESP_LOGE(TAG, "Lua error: %s", msg ? msg : "unknown");
        lua_pop(L, 1); /* 移除错误信息 */
    }
}

/* ------------------------------------------------------------------ */
/* sys 模块：系统基础功能                                             */
/* ------------------------------------------------------------------ */

/** sys.log(tag, msg) — 打印日志 */
static int sys_log(lua_State *L)
{
    const char *tag = luaL_checkstring(L, 1);
    const char *msg = luaL_checkstring(L, 2);
    ESP_LOGI(tag, "%s", msg);
    return 0;
}

/** sys.delay(ms) — 延时 */
static int sys_delay(lua_State *L)
{
    int ms = luaL_checkinteger(L, 1);
    if (ms < 0) ms = 0;
    vTaskDelay(pdMS_TO_TICKS(ms));
    return 0;
}

/** sys.reboot() — 重启系统 */
static int sys_reboot(lua_State *L)
{
    (void)L;
    ESP_LOGW(TAG, "Lua requested system reboot");
    esp_restart();
    return 0; /* 不会走到这里 */
}

/** sys.tick_ms() — 获取系统 tick（毫秒） */
static int sys_tick_ms(lua_State *L)
{
    lua_pushinteger(L, (lua_Integer)(xTaskGetTickCount() * portTICK_PERIOD_MS));
    return 1;
}

/** sys.meminfo() — 返回已用内存（KB） */
static int sys_meminfo(lua_State *L)
{
    lua_pushinteger(L, (lua_Integer)esp_get_free_heap_size());
    return 1;
}

static const luaL_Reg sys_lib[] = {
    {"log",      sys_log},
    {"delay",    sys_delay},
    {"reboot",   sys_reboot},
    {"tick_ms",  sys_tick_ms},
    {"meminfo",  sys_meminfo},
    {NULL, NULL}
};

/* ------------------------------------------------------------------ */
/* device 模块：设备框架绑定                                          */
/* ------------------------------------------------------------------ */
/* （实现在 lua_device.c 中，这里只声明注册函数） */
extern int luaopen_device(lua_State *L);

/* ------------------------------------------------------------------ */
/* 公开 API                                                           */
/* ------------------------------------------------------------------ */

int lua_runtime_init(void)
{
    if (s_L != NULL) {
        ESP_LOGW(TAG, "Lua runtime already initialized");
        return 0;
    }

    /* 创建 Lua VM */
    s_L = luaL_newstate();
    if (s_L == NULL) {
        ESP_LOGE(TAG, "Failed to create Lua state (out of memory?)");
        return -1;
    }

    /* 设置 panic 回调 */
    lua_atpanic(s_L, panic_cb);

    /* 打开标准库（按需开启，省内存） */
    luaL_openlibs(s_L);

    /* 注册 sys 模块 */
    luaL_newlib(s_L, sys_lib);
    lua_setglobal(s_L, "sys");

    /* 注册 device 模块 */
    luaL_requiref(s_L, "device", luaopen_device, 1);
    lua_pop(s_L, 1); /* luaL_requiref 把模块留在栈上，弹掉 */

    ESP_LOGI(TAG, "Lua runtime initialized (Lua %s)", LUA_RELEASE);
    return 0;
}

int lua_run_file(const char *path)
{
    if (s_L == NULL) {
        ESP_LOGE(TAG, "Lua runtime not initialized");
        return -1;
    }
    if (path == NULL) {
        ESP_LOGE(TAG, "Invalid path");
        return -1;
    }

    ESP_LOGI(TAG, "Running Lua script: %s", path);

    int status = luaL_dofile(s_L, path);
    if (status != LUA_OK) {
        report_error(s_L, status);
        return -1;
    }

    return 0;
}

int lua_run_string(const char *code)
{
    if (s_L == NULL) {
        ESP_LOGE(TAG, "Lua runtime not initialized");
        return -1;
    }
    if (code == NULL)
        return -1;

    int status = luaL_dostring(s_L, code);
    if (status != LUA_OK) {
        report_error(s_L, status);
        return -1;
    }

    return 0;
}

void lua_runtime_deinit(void)
{
    if (s_L != NULL) {
        lua_close(s_L);
        s_L = NULL;
        ESP_LOGI(TAG, "Lua runtime deinitialized");
    }
}
