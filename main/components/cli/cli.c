/**
 * @file cli.c
 * @brief 命令行接口实现
 *
 * 通过控制台（UART0 / USB Serial JTAG）提供交互式命令行。
 * 支持行编辑、命令表扩展。
 */
#include "cli.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "lua.h"
#include "lauxlib.h"
#include "lua_runtime.h"
#include "framework.h"

static const char *TAG = "cli";

/* ===== 配置 ===== */

#define CLI_BUF_SIZE 128 /* 单行最大字符数 */
#define CLI_MAX_ARGS 16  /* 单条命令最大参数数 */
#define CLI_MAX_CMDS 32  /* 最多注册命令数 */
#define CLI_TASK_STACK 4096
#define CLI_TASK_PRIO 2
#define CLI_PROMPT "aivox> "

/* ===== 命令表结构 ===== */

struct cmd_entry
{
    const char *name;
    cli_cmd_handler_t handler;
    const char *help;
};

static struct cmd_entry s_cmd_table[CLI_MAX_CMDS];
static int s_cmd_count = 0;

/* ------------------------------------------------------------------ */
/* 命令表操作                                                         */
/* ------------------------------------------------------------------ */

int cli_register_cmd(const char *name, cli_cmd_handler_t handler, const char *help)
{
    if (name == NULL || handler == NULL)
        return -1;

    for (int i = 0; i < s_cmd_count; i++)
    {
        if (strcmp(s_cmd_table[i].name, name) == 0)
        {
            ESP_LOGW(TAG, "Command '%s' already registered", name);
            return -1;
        }
    }

    if (s_cmd_count >= CLI_MAX_CMDS)
    {
        ESP_LOGE(TAG, "Command table full");
        return -1;
    }

    s_cmd_table[s_cmd_count].name = name;
    s_cmd_table[s_cmd_count].handler = handler;
    s_cmd_table[s_cmd_count].help = help ? help : "";
    s_cmd_count++;

    return 0;
}

static struct cmd_entry *find_cmd(const char *name)
{
    for (int i = 0; i < s_cmd_count; i++)
    {
        if (strcmp(s_cmd_table[i].name, name) == 0)
            return &s_cmd_table[i];
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* 命令行解析                                                         */
/* ------------------------------------------------------------------ */

static int split_args(char *line, char *argv[], int max_args)
{
    int argc = 0;
    char *p = line;

    while (*p && argc < max_args)
    {
        while (*p && isspace((unsigned char)*p))
            p++;

        if (*p == '\0')
            break;

        argv[argc++] = p;

        while (*p && !isspace((unsigned char)*p))
            p++;

        if (*p != '\0')
        {
            *p = '\0';
            p++;
        }
    }

    return argc;
}

int cli_exec_line(char *line)
{
    if (line == NULL || *line == '\0')
        return 0;

    int len = strlen(line);
    while (len > 0 && isspace((unsigned char)line[len - 1]))
    {
        line[--len] = '\0';
    }
    if (len == 0)
        return 0;

    char *argv[CLI_MAX_ARGS];
    int argc = split_args(line, argv, CLI_MAX_ARGS);

    if (argc == 0)
        return 0;

    struct cmd_entry *cmd = find_cmd(argv[0]);
    if (cmd == NULL)
    {
        printf("Unknown command: %s\r\n", argv[0]);
        printf("Type 'help' for available commands.\r\n");
        return -1;
    }

    return cmd->handler(argc, argv);
}

/* ------------------------------------------------------------------ */
/* 辅助回调（供 device_for_each 使用）                               */
/* ------------------------------------------------------------------ */

static void count_devices_cb(struct Device *dev, void *arg)
{
    (void)dev;
    int *count = (int *)arg;
    (*count)++;
}

static void print_devices_cb(struct Device *dev, void *arg)
{
    (void)arg;
    const char *ops_str = "none";
    if (dev->ops)
    {
        static char buf[8];
        int pos = 0;
        if (dev->ops->init)
            buf[pos++] = 'I';
        if (dev->ops->read)
            buf[pos++] = 'R';
        if (dev->ops->write)
            buf[pos++] = 'W';
        if (dev->ops->deinit)
            buf[pos++] = 'D';
        buf[pos] = '\0';
        ops_str = buf;
    }
    printf("  %-16s ops: %s\r\n", dev->name, ops_str);
}

/* ------------------------------------------------------------------ */
/* 内置命令                                                           */
/* ------------------------------------------------------------------ */

static int cmd_help(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("\r\nAvailable commands:\r\n");
    for (int i = 0; i < s_cmd_count; i++)
    {
        printf("  %-12s %s\r\n", s_cmd_table[i].name, s_cmd_table[i].help);
    }
    printf("\r\n");
    return 0;
}

static int cmd_echo(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        if (i > 1)
            printf(" ");
        printf("%s", argv[i]);
    }
    printf("\r\n");
    return 0;
}

static int cmd_clear(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    printf("\033[2J\033[H");
    return 0;
}

static int cmd_reboot(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    printf("Rebooting...\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return 0;
}

static int cmd_info(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("\r\n--- System Info ---\r\n");
    printf("  Chip:         ESP32-S3\r\n");
    printf("  Free heap:    %lu bytes\r\n", (unsigned long)esp_get_free_heap_size());
    printf("  Free PSRAM:   %lu bytes\r\n", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    printf("  Uptime:       %lu ms\r\n", (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS));

    int count = 0;
    device_for_each(count_devices_cb, &count);
    printf("  Devices:      %d\r\n", count);

    printf("\r\n");
    return 0;
}

static int cmd_ls(int argc, char *argv[])
{
    const char *path = "/sdcard";
    if (argc >= 2)
        path = argv[1];

    DIR *d = opendir(path);
    if (d == NULL)
    {
        printf("Cannot open directory: %s\r\n", path);
        return -1;
    }

    printf("\r\nListing: %s\r\n", path);
    struct dirent *ent;
    int count = 0;
    while ((ent = readdir(d)) != NULL)
    {
        printf("  %s\r\n", ent->d_name);
        count++;
    }
    closedir(d);
    printf("  (%d entries)\r\n\r\n", count);
    return 0;
}

static int cmd_run(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: run <script.lua>\r\n");
        return -1;
    }

    char path[256];
    if (argv[1][0] == '/')
    {
        snprintf(path, sizeof(path), "%s", argv[1]);
    }
    else
    {
        snprintf(path, sizeof(path), "/sdcard/%s", argv[1]);
    }

    printf("Running: %s\r\n", path);
    int ret = lua_run_file(path);
    if (ret != 0)
    {
        printf("Script exited with error.\r\n");
        return -1;
    }

    printf("Script finished.\r\n");
    return 0;
}

static int cmd_devices(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("\r\nRegistered devices:\r\n");
    device_for_each(print_devices_cb, NULL);
    printf("\r\n");
    return 0;
}

/* ------------------------------------------------------------------ */
/* 注册内置命令                                                       */
/* ------------------------------------------------------------------ */

static void register_builtin_cmds(void)
{
    cli_register_cmd("help", cmd_help, "Show available commands");
    cli_register_cmd("echo", cmd_echo, "Echo text back");
    cli_register_cmd("clear", cmd_clear, "Clear the screen");
    cli_register_cmd("reboot", cmd_reboot, "Reboot the system");
    cli_register_cmd("info", cmd_info, "Show system information");
    cli_register_cmd("ls", cmd_ls, "List files [path]");
    cli_register_cmd("run", cmd_run, "Run a Lua script file");
    cli_register_cmd("devices", cmd_devices, "List registered devices");
}

/* ------------------------------------------------------------------ */
/* CLI 任务                                                           */
/* ------------------------------------------------------------------ */

static void cli_task(void *arg)
{
    (void)arg;

    char line[CLI_BUF_SIZE];

    printf("\r\n");
    printf("========================================\r\n");
    printf("  AI-VOX3 Shell (Lua powered)\r\n");
    printf("  Type 'help' for commands\r\n");
    printf("========================================\r\n");
    printf("\r\n");

    /* 尝试自动执行 SD 卡中的 startup.lua */
    {
        FILE *f = fopen("/sdcard/startup.lua", "r");
        if (f != NULL)
        {
            fclose(f);
            printf("Running /sdcard/startup.lua ...\r\n");
            lua_run_file("/sdcard/startup.lua");
            printf("\r\n");
        }
    }

    while (1)
    {
        printf("%s", CLI_PROMPT);
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        /* 去掉末尾换行 */
        int len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        {
            line[--len] = '\0';
        }

        if (len > 0)
        {
            cli_exec_line(line);
        }
    }
}

/* ------------------------------------------------------------------ */
/* 初始化                                                             */
/* ------------------------------------------------------------------ */

int cli_init(void)
{
    register_builtin_cmds();

    /* 如果 Lua 运行时已就绪，注册 cli Lua 模块 */
    lua_State *L = (lua_State *)lua_runtime_get_state();
    if (L != NULL)
    {
        extern int luaopen_cli(lua_State * L);
        luaL_requiref(L, "cli", luaopen_cli, 1);
        lua_pop(L, 1); /* 弹出模块表 */
        ESP_LOGI(TAG, "Lua 'cli' module registered");
    }

    BaseType_t ret = xTaskCreate(
        cli_task, "cli_task",
        CLI_TASK_STACK, NULL, CLI_TASK_PRIO, NULL);

    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create CLI task");
        return -1;
    }

    ESP_LOGI(TAG, "CLI initialized");
    return 0;
}
