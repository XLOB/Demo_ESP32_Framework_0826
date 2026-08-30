/**
 * @file cli.c
 * @brief 命令行接口实现（基于 esp_console）
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
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "lua.h"
#include "lauxlib.h"
#include "lua_runtime.h"
#include "framework/framework.h"
#include "esp_chip_info.h"

static const char *TAG = "cli";

#define CLI_BUF_SIZE 256
#define CLI_MAX_CMDS 32
#define CLI_TASK_STACK 4096
#define CLI_TASK_PRIO 5
#define CLI_PROMPT "aivox> "
#define CLI_HISTORY_PATH "/sdcard/.cli_history"

/* ===== 命令表 ===== */
struct cmd_entry
{
    const char *name;
    cli_cmd_handler_t handler;
    const char *help;
};

static struct cmd_entry s_cmd_table[CLI_MAX_CMDS];
static int s_cmd_count = 0;

/* ------------------------------------------------------------------ */
/* 命令注册                                                           */
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
/* 命令解析                                                           */
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

    char *argv[16];
    int argc = split_args(line, argv, 16);

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

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    const char *chip_model = "Unknown";
    switch (chip_info.model)
    {
    case CHIP_ESP32:
        chip_model = "ESP32";
        break;
    case CHIP_ESP32S2:
        chip_model = "ESP32-S2";
        break;
    case CHIP_ESP32S3:
        chip_model = "ESP32-S3";
        break;
    case CHIP_ESP32C3:
        chip_model = "ESP32-C3";
        break;
    case CHIP_ESP32C6:
        chip_model = "ESP32-C6";
        break;
    case CHIP_ESP32H2:
        chip_model = "ESP32-H2";
        break;
    case CHIP_ESP32C2:
        chip_model = "ESP32-C2";
        break;
    case CHIP_ESP32C61:
        chip_model = "ESP32-C61";
        break;
    case CHIP_ESP32C5:
        chip_model = "ESP32-C5";
        break;
    case CHIP_ESP32P4:
        chip_model = "ESP32-P4";
        break;
    default:
        chip_model = "Unknown";
        break;
    }

    printf("\r\n--- System Info ---\r\n");
    printf("  Chip:         %s (rev %d)\r\n", chip_model, chip_info.revision);
    printf("  Free heap:    %lu bytes\r\n", (unsigned long)esp_get_free_heap_size());
    printf("  Free PSRAM:   %lu bytes\r\n", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    printf("\r\n");
    return 0;
}

static int cmd_ls(int argc, char *argv[])
{
    const char *path = (argc > 1) ? argv[1] : "/sdcard";
    DIR *d = opendir(path);
    if (d == NULL)
    {
        printf("Cannot open directory: %s\r\n", path);
        return -1;
    }
    printf("\r\n%s:\r\n", path);
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL)
    {
        printf("  %s\r\n", ent->d_name);
    }
    closedir(d);
    printf("\r\n");
    return 0;
}

static int cmd_lua(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: lua <filename>\r\n");
        return -1;
    }
    return lua_run_file(argv[1]);
}

static void register_builtin_cmds(void)
{
    cli_register_cmd("help", cmd_help, "Show this help");
    cli_register_cmd("echo", cmd_echo, "Echo text");
    cli_register_cmd("clear", cmd_clear, "Clear screen");
    cli_register_cmd("reboot", cmd_reboot, "Reboot system");
    cli_register_cmd("info", cmd_info, "System info");
    cli_register_cmd("ls", cmd_ls, "List directory");
    cli_register_cmd("lua", cmd_lua, "Run a Lua script");
}

/* ------------------------------------------------------------------ */
/* esp_console 命令包装（把我们的命令注册到 esp_console）             */
/* ------------------------------------------------------------------ */

static int console_cmd_wrapper(int argc, char **argv)
{
    /* esp_console 的 argv[0] 是命令名，跟我们的格式一致 */
    struct cmd_entry *cmd = find_cmd(argv[0]);
    if (cmd == NULL)
    {
        return -1;
    }
    return cmd->handler(argc, argv);
}

static void register_all_to_esp_console(void)
{
    for (int i = 0; i < s_cmd_count; i++)
    {
        esp_console_cmd_t cmd = {
            .command = s_cmd_table[i].name,
            .help = s_cmd_table[i].help,
            .hint = NULL,
            .func = &console_cmd_wrapper,
        };
        esp_console_cmd_register(&cmd);
    }
}

/* ------------------------------------------------------------------ */
/* CLI 主循环（用 esp_console_repl）                                  */
/* ------------------------------------------------------------------ */

static void cli_task(void *arg)
{
    (void)arg;

    printf("\r\n");
    printf("========================================\r\n");
    printf("  AI-VOX3 Shell (Lua powered)\r\n");
    printf("  Type 'help' for commands\r\n");
    printf("========================================\r\n");
    printf("\r\n");

    /* 自动执行 startup.lua */
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

    /* 启动 REPL（阻塞，内部循环读输入） */
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = CLI_PROMPT;
    repl_config.history_save_path = CLI_HISTORY_PATH;

    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

    /* 先试试 UART 控制台 */
    esp_err_t ret = esp_console_new_repl_uart(&uart_config, &repl_config, &repl);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "UART console not available (%s), trying USB CDC",
                 esp_err_to_name(ret));

        /* UART 不行就用 USB Serial JTAG */
        esp_console_dev_usb_serial_jtag_config_t usb_config =
            ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
        ret = esp_console_new_repl_usb_serial_jtag(&usb_config, &repl_config, &repl);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "No console device available: %s",
                     esp_err_to_name(ret));
            return;
        }
    }

    /* 把所有命令注册到 esp_console */
    register_all_to_esp_console();

    /* 启动 REPL（这个函数内部循环，不会返回） */
    esp_console_start_repl(repl);

    /* 不会到这里 */
    vTaskDelete(NULL);
}

/* ------------------------------------------------------------------ */
/* 初始化                                                             */
/* ------------------------------------------------------------------ */

int cli_init(void)
{
    register_builtin_cmds();

    /* 注册 cli Lua 模块 */
    lua_State *L = (lua_State *)lua_runtime_get_state();
    if (L != NULL)
    {
        extern int luaopen_cli(lua_State * L);
        luaL_requiref(L, "cli", luaopen_cli, 1);
        lua_pop(L, 1);
        ESP_LOGI(TAG, "Lua 'cli' module registered");
    }

    /* 初始化 esp_console */
    esp_console_config_t console_config = {
        .max_cmdline_args = 16,
        .max_cmdline_length = CLI_BUF_SIZE,
    };
    esp_console_init(&console_config);

    BaseType_t ret = xTaskCreate(
        cli_task, "cli_task",
        CLI_TASK_STACK * 4, NULL, CLI_TASK_PRIO, NULL);

    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create CLI task");
        return -1;
    }

    ESP_LOGI(TAG, "CLI initialized");
    return 0;
}