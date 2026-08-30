/**
 * @file lua_cli.c
 * @brief Lua cli 模块 —— 命令行接口绑定
 *
 * 将 CLI 组件的命令注册功能暴露给 Lua。
 * Lua 侧用法：
 *   cli.register("mycmd", function(args)
 *       print("args count:", #args)
 *       for i, v in ipairs(args) do print(i, v) end
 *   end, "My custom command")
 *
 *   cli.exec("ls /sdcard")  -- 执行一条命令
 */
#include "lua.h"
#include "lauxlib.h"
#include "cli.h"
#include "lua_runtime.h"
#include "esp_log.h"

static const char *TAG = "lua_cli";

/** 注册到 CLI 的 Lua 命令最大数量 */
#define MAX_LUA_CMDS    32

/**
 * @brief Lua 命令记录
 *
 * 每个 Lua 命令对应一条记录，存储 Lua 函数在 registry 中的引用。
 */
struct lua_cmd_entry {
    int  ref;       /* Lua 函数在 registry 中的引用（LUA_REFNIL 表示空闲） */
    char name[32];  /* 命令名，方便调试 */
};

static struct lua_cmd_entry s_lua_cmds[MAX_LUA_CMDS];

/* ------------------------------------------------------------------ */
/* 内部工具                                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief 查找一个空闲的 Lua 命令槽位
 * @return 索引，-1 表示已满
 */
static int find_free_slot(void)
{
    for (int i = 0; i < MAX_LUA_CMDS; i++) {
        if (s_lua_cmds[i].ref == LUA_REFNIL)
            return i;
    }
    return -1;
}

/**
 * @brief 通用 C 回调：被 CLI 调用时，转发到对应的 Lua 函数
 *
 * 这是所有 Lua 命令共享的 C 处理函数。通过 argc 的"用户数据"
 * 机制来找到对应的 Lua 函数？不，CLI 的 handler 签名是固定的。
 * 我们用命令名来查找：遍历 s_lua_cmds 找到名字匹配的条目。
 *
 * 等等，argv[0] 就是命令名，所以我们可以用命令名来反向查找。
 */
static int lua_cmd_dispatcher(int argc, char *argv[])
{
    if (argc < 1 || argv[0] == NULL)
        return -1;

    const char *cmd_name = argv[0];

    /* 找到对应的 Lua 命令条目 */
    int slot = -1;
    for (int i = 0; i < MAX_LUA_CMDS; i++) {
        if (s_lua_cmds[i].ref != LUA_REFNIL &&
            strcmp(s_lua_cmds[i].name, cmd_name) == 0) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        ESP_LOGE(TAG, "Lua command '%s' not found (should not happen)", cmd_name);
        return -1;
    }

    /* 获取 Lua state */
    lua_State *L = (lua_State *)lua_runtime_get_state();
    if (L == NULL) {
        ESP_LOGE(TAG, "Lua state not available");
        return -1;
    }

    /* 从 registry 取出 Lua 函数 */
    lua_rawgeti(L, LUA_REGISTRYINDEX, s_lua_cmds[slot].ref);
    if (!lua_isfunction(L, -1)) {
        ESP_LOGE(TAG, "Registry ref %d is not a function", s_lua_cmds[slot].ref);
        lua_pop(L, 1);
        return -1;
    }

    /* 把参数打包成一个 Lua table 压入栈（作为函数的第一个参数）
       args[1] = argv[1], args[2] = argv[2], ...
       注意：不包含命令名本身（argv[0]），因为 Lua 侧不需要知道命令名 */
    lua_newtable(L);
    for (int i = 1; i < argc; i++) {
        lua_pushinteger(L, (lua_Integer)i);
        lua_pushstring(L, argv[i]);
        lua_settable(L, -3);
    }

    /* 调用 Lua 函数：1 个参数（args table），1 个返回值 */
    int status = lua_pcall(L, 1, 1, 0);
    if (status != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        printf("Lua command error: %s\r\n", err ? err : "unknown");
        lua_pop(L, 1);
        return -1;
    }

    /* 读取返回值：如果是数字，作为返回码；否则默认返回 0 */
    int ret = 0;
    if (lua_isnumber(L, -1)) {
        ret = (int)lua_tointeger(L, -1);
    }
    lua_pop(L, 1);

    return ret;
}

/* ------------------------------------------------------------------ */
/* cli 模块函数                                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief cli.register(name, func, help) — 注册一条命令
 *
 * @param name   命令名（字符串）
 * @param func   Lua 函数，参数是 args table，返回 0 表示成功
 * @param help   帮助文字（可选，字符串）
 * @return 成功返回 true，失败返回 nil + 错误信息
 */
static int cli_register(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    const char *help = luaL_optstring(L, 3, "");

    /* 找一个空闲槽位 */
    int slot = find_free_slot();
    if (slot < 0) {
        lua_pushnil(L);
        lua_pushstring(L, "too many Lua commands registered");
        return 2;
    }

    /* 把函数复制到栈顶（因为 luaL_checktype 只是检查，函数可能在任意位置）
       实际上它就在 2 号位置，直接引用即可 */
    lua_pushvalue(L, 2);

    /* 存入 registry，拿到引用 */
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    /* 注册到 CLI */
    int ret = cli_register_cmd(name, lua_cmd_dispatcher, help);
    if (ret != 0) {
        /* 注册失败，释放引用 */
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        lua_pushnil(L);
        lua_pushstring(L, "failed to register command (name exists?)");
        return 2;
    }

    /* 保存记录 */
    s_lua_cmds[slot].ref = ref;
    strncpy(s_lua_cmds[slot].name, name, sizeof(s_lua_cmds[slot].name) - 1);
    s_lua_cmds[slot].name[sizeof(s_lua_cmds[slot].name) - 1] = '\0';

    ESP_LOGD(TAG, "Lua command registered: %s (slot %d, ref %d)", name, slot, ref);

    lua_pushboolean(L, 1);
    return 1;
}

/**
 * @brief cli.exec(line) — 执行一条命令行
 *
 * 相当于在命令行输入 line 并回车。
 * 返回命令执行结果（0 成功，-1 失败）。
 */
static int cli_exec(lua_State *L)
{
    const char *line = luaL_checkstring(L, 1);

    /* cli_exec_line 会修改字符串（按空格分割），所以复制一份 */
    char buf[256];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    int ret = cli_exec_line(buf);
    lua_pushinteger(L, (lua_Integer)ret);
    return 1;
}

/** cli 模块函数表 */
static const luaL_Reg cli_lib[] = {
    {"register", cli_register},
    {"exec",     cli_exec},
    {NULL, NULL}
};

/* ------------------------------------------------------------------ */
/* 模块入口                                                           */
/* ------------------------------------------------------------------ */

int luaopen_cli(lua_State *L)
{
    /* 初始化 Lua 命令表 */
    for (int i = 0; i < MAX_LUA_CMDS; i++) {
        s_lua_cmds[i].ref = LUA_REFNIL;
        s_lua_cmds[i].name[0] = '\0';
    }

    luaL_newlib(L, cli_lib);
    return 1;
}
