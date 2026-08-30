/**
 * @file lua_device.c
 * @brief Lua device 模块 —— 设备框架绑定
 *
 * 将框架的 struct Device 暴露为 Lua userdata 对象。
 * Lua 侧用法：
 *   local dev = device.find("internal_temp")
 *   if dev then
 *       local data = dev:read(4)    -- 读4字节
 *       dev:write("hello")           -- 写数据
 *       print(dev.name)              -- 设备名
 *   end
 *
 *   device.list()                     -- 返回所有设备名的 table
 */
#include "lua.h"
#include "lauxlib.h"
#include "framework.h"
#include "esp_log.h"

static const char *TAG = "lua_device";

/** Lua 中 device 对象的 metatable 名称 */
#define DEVICE_MT "Device"

/* ------------------------------------------------------------------ */
/* 辅助函数                                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief 把一个 Device 指针作为 userdata 压入栈
 * @return 压入栈的 userdata 指针
 */
static struct Device **push_device(lua_State *L, struct Device *dev)
{
    struct Device **ud = (struct Device **)lua_newuserdata(L, sizeof(struct Device *));
    *ud = dev;
    luaL_setmetatable(L, DEVICE_MT);
    return ud;
}

/**
 * @brief 检查栈上第 n 个参数是否为 Device userdata
 */
static struct Device *check_device(lua_State *L, int n)
{
    struct Device **ud = (struct Device **)luaL_checkudata(L, n, DEVICE_MT);
    luaL_argcheck(L, *ud != NULL, n, "device is null");
    return *ud;
}

/* ------------------------------------------------------------------ */
/* Device 方法                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief dev:read(len) — 读设备数据
 *
 * 返回字符串（读到的数据），失败返回 nil + 错误信息
 */
static int device_read(lua_State *L)
{
    struct Device *dev = check_device(L, 1);
    int len = luaL_checkinteger(L, 2);

    if (len <= 0 || len > 4096)
    {
        lua_pushnil(L);
        lua_pushstring(L, "invalid read length (1..4096)");
        return 2;
    }

    if (dev->ops == NULL || dev->ops->read == NULL)
    {
        lua_pushnil(L);
        lua_pushstring(L, "device does not support read");
        return 2;
    }

    char *buf = (char *)lua_newuserdata(L, (size_t)len);
    int ret = dev->ops->read(dev->data, buf, (size_t)len);

    if (ret < 0)
    {
        lua_pop(L, 1); /* 弹出 buf */
        lua_pushnil(L);
        lua_pushstring(L, "read failed");
        return 2;
    }

    /* 把读到的数据作为 Lua 字符串返回 */
    lua_pushlstring(L, buf, (size_t)ret);
    return 1;
}

/**
 * @brief dev:write(data) — 写设备数据
 *
 * data 可以是字符串或数字。
 * 返回成功写入的字节数，失败返回 nil + 错误信息
 */
static int device_write(lua_State *L)
{
    struct Device *dev = check_device(L, 1);
    size_t len = 0;
    const char *data = luaL_checklstring(L, 2, &len);

    if (dev->ops == NULL || dev->ops->write == NULL)
    {
        lua_pushnil(L);
        lua_pushstring(L, "device does not support write");
        return 2;
    }

    int ret = dev->ops->write(dev->data, data, len);
    if (ret < 0)
    {
        lua_pushnil(L);
        lua_pushstring(L, "write failed");
        return 2;
    }

    lua_pushinteger(L, (lua_Integer)ret);
    return 1;
}

/**
 * @brief dev.name — 访问设备名（属性）
 */
static int device_index(lua_State *L)
{
    struct Device *dev = check_device(L, 1);
    const char *key = luaL_checkstring(L, 2);

    if (strcmp(key, "name") == 0)
    {
        lua_pushstring(L, dev->name);
        return 1;
    }

    /* 查找 metatable 里的方法（read、write 等） */
    luaL_getmetatable(L, DEVICE_MT);
    lua_getfield(L, -1, key);
    if (!lua_isnil(L, -1))
    {
        return 1;
    }

    lua_pop(L, 2); /* 弹出 nil 和 metatable */
    lua_pushnil(L);
    return 1;
}

/** Device metatable 方法表 */
static const luaL_Reg device_mt[] = {
    {"__index", device_index},
    {"read", device_read},
    {"write", device_write},
    {NULL, NULL}};

/* ------------------------------------------------------------------ */
/* device 模块函数                                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief device.find(name) — 按名称查找设备
 *
 * 找到返回 Device 对象，未找到返回 nil
 */
static int mod_device_find(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    struct Device *dev = device_find(name);

    if (dev == NULL)
    {
        lua_pushnil(L);
        return 1;
    }

    push_device(L, dev);
    return 1;
}

/** 遍历回调：收集设备名 */
struct list_ctx
{
    lua_State *L;
    int index;
};

static void list_cb(struct Device *dev, void *arg)
{
    struct list_ctx *ctx = (struct list_ctx *)arg;
    ctx->index++;
    lua_pushinteger(ctx->L, (lua_Integer)ctx->index);
    lua_pushstring(ctx->L, dev->name);
    lua_settable(ctx->L, -3);
}

/**
 * @brief device.list() — 列出所有已注册设备
 *
 * 返回一个数组 table，元素是设备名字符串
 */
static int mod_device_list(lua_State *L)
{
    lua_newtable(L); /* 返回的 table */

    struct list_ctx ctx = {.L = L, .index = 0};
    device_for_each(list_cb, &ctx);

    return 1;
}

/** 遍历回调：对每个设备调用 Lua 函数 */
struct foreach_ctx
{
    lua_State *L;
    int func_idx; /* 回调函数在栈上的位置（相对位置已修正） */
};

static void foreach_cb(struct Device *dev, void *arg)
{
    struct foreach_ctx *ctx = (struct foreach_ctx *)arg;
    lua_State *L = ctx->L;

    /* 复制回调函数到栈顶 */
    lua_pushvalue(L, ctx->func_idx);

    /* 把 device userdata 作为参数压入 */
    push_device(L, dev);

    /* 调用回调函数（1个参数，0个返回值） */
    lua_call(L, 1, 0);
}

/**
 * @brief device.foreach(func) — 遍历所有设备
 *
 * func(dev) 对每个设备调用一次
 */
static int mod_device_foreach(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);

    /* 回调函数在栈上的绝对索引（device_for_each 会多次调用，
       需要保证函数一直在栈上） */
    int func_idx = lua_gettop(L);

    struct foreach_ctx ctx = {.L = L, .func_idx = func_idx};
    device_for_each(foreach_cb, &ctx);

    return 0;
}

/** device 模块函数表 */
static const luaL_Reg device_lib[] = {
    {"find", mod_device_find},
    {"list", mod_device_list},
    {"foreach", mod_device_foreach},
    {NULL, NULL}};

/* ------------------------------------------------------------------ */
/* 模块入口                                                           */
/* ------------------------------------------------------------------ */

int luaopen_device(lua_State *L)
{
    /* 创建 Device metatable */
    luaL_newmetatable(L, DEVICE_MT);
    luaL_setfuncs(L, device_mt, 0);
    lua_pop(L, 1); /* 弹出 metatable */

    /* 创建 device 模块表 */
    luaL_newlib(L, device_lib);
    return 1;
}
