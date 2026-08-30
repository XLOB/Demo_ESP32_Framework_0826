/**
 * @file main.c
 * @brief 应用入口
 *
 * 启动流程：
 *   1. 注册所有设备驱动
 *   2. 统一初始化所有设备
 *   3. 创建按键消息队列
 *   4. 初始化命令处理器（消费按键事件）
 *   5. 注册额外命令回调（LED 颜色切换等）
 *   6. 初始化 Lua 运行时
 *   7. 启动 CLI（命令行界面，自动运行）
 *   8. 启动各应用任务
 */
#include "framework/framework.h"

#include "drivers/key.h"
#include "drivers/internal_temp.h"
#include "drivers/battery.h"
#include "drivers/sys_uptime.h"
#include "drivers/display.h"
#include "drivers/backlight.h"
#include "drivers/uart_ph2.h"
#include "drivers/sensor.h"
#include "drivers/sd_card.h"
#include "drivers/ws2812b.h"

#include "app/app_key_task.h"
#include "app/app_temp_task.h"
#include "app/app_ui_task.h"

#include "components/command_handler.h"
#include "lua_runtime.h"
// #include "cli.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "main";

/* ===== 任务配置 ===== */

#define TASK_STACK_SIZE 4096
#define KEY_TASK_PRIO 5
#define CMD_TASK_PRIO 4 /* 与 command_handler 内部任务一致 */
#define TEMP_TASK_PRIO 4
#define UI_TASK_PRIO 3

/* ===== 全局变量 ===== */

/** 按键消息队列（生产者：key_task；消费者：command_handler） */
QueueHandle_t key_queue;

/* ------------------------------------------------------------------ */
/* LED 颜色切换命令（注册到 command_handler）                         */
/* ------------------------------------------------------------------ */

/**
 * @brief 命令：循环切换 WS2812B LED 颜色
 *
 * 颜色循环：红 → 绿 → 蓝 → 灭 → 红 ...
 * 绑定到 key_a（与背光切换共享同一按键）。
 */
static void cmd_led_color_toggle(void *arg)
{
    ESP_LOGI(TAG, "======xhyOS======");
    (void)arg;

    static int color_index = 3; /* 初始为灭（与 LED 初始状态一致） */

    struct Device *led_dev = device_find("ws2812b");
    if (!led_dev || !led_dev->ops || !led_dev->ops->write)
        return;

    /* 切换到下一个颜色 */
    color_index = (color_index + 1) % 4;

    static const uint8_t colors[4][3] = {
        {255, 0, 0}, /* 红 */
        {0, 255, 0}, /* 绿 */
        {0, 0, 255}, /* 蓝 */
        {0, 0, 0},   /* 灭 */
    };

    led_dev->ops->write(led_dev->data, colors[color_index], 3);
    ESP_LOGD(TAG, "LED 颜色切换: 索引 %d", color_index);
}

/* ------------------------------------------------------------------ */
/* app_main                                                           */
/* ------------------------------------------------------------------ */

void app_main(void)
{
    /* ---- 1. 注册所有设备 ---- */

    device_register(Key_create(46, "key_a"));
    device_register(Key_create(45, "key_b"));
    device_register(InternalTemp_get_device());
    device_register(Battery_get_device());
    device_register(SysUptime_get_device());
    device_register(Display_get_device());
    device_register(Backlight_get_device());
    device_register(UartPh2_get_device());
    device_register(VirtualSensor_get_device());
    device_register(SdCard_get_device());
    device_register(Ws2812b_get_device());

    /* ---- 2. 统一初始化所有设备 ---- */

    int init_failed = device_init_all();
    if (init_failed < 0)
    {
        ESP_LOGE(TAG, "Device framework initialization failed");
        return;
    }
    if (init_failed > 0)
    {
        ESP_LOGW(TAG, "%d device(s) failed to initialize, continuing with remaining devices", init_failed);
    }

    /* ---- 3. 创建按键消息队列 ---- */

    key_queue = xQueueCreate(10, sizeof(int));
    if (key_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create key queue");
        return;
    }

    /* ---- 4. 初始化命令处理器 ---- */

    command_handler_init();

    /* key_a 同时触发背光切换（默认）和 LED 颜色切换 */
    command_handler_register(CMD_KEY_A_SHORT, cmd_led_color_toggle, NULL);

    /* ---- 5. 初始化 Lua 运行时 ---- */

    if (lua_runtime_init() != 0)
    {
        ESP_LOGE(TAG, "Lua runtime init failed");
    }

    /* ---- 6. 启动 CLI（命令行自动运行） ---- */

    //  cli_init();

    /* ---- 7. 启动应用任务 ---- */

    xTaskCreate(key_task, "key_task", TASK_STACK_SIZE, NULL, KEY_TASK_PRIO, NULL);
    xTaskCreate(temp_task, "temp_task", TASK_STACK_SIZE, NULL, TEMP_TASK_PRIO, NULL);
    xTaskCreate(ui_task, "ui_task", TASK_STACK_SIZE, NULL, UI_TASK_PRIO, NULL);

    ESP_LOGI(TAG, "System started");
}
