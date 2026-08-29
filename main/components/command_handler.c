/**
 * @file command_handler.c
 * @brief 命令处理器实现
 *
 * 设计要点：
 * - 作为按键事件的唯一消费者，避免多任务竞争同一队列
 * - 每个命令 ID 可挂多个回调，触发时按注册顺序执行
 * - 回调在 cmd_handler 任务上下文中执行，注意不要阻塞过久
 */
#include "command_handler.h"
#include "framework/framework.h"
#include "drivers/backlight.h"
#include "drivers/battery.h"
#include "drivers/internal_temp.h"
#include "drivers/sys_uptime.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "cmd_handler";

/* 外部按键消息队列（在 main.c 中定义） */
extern QueueHandle_t key_queue;

/* ===== 回调表配置 ===== */

#define MAX_CALLBACKS_PER_CMD  4   ///< 每个命令最多的回调数
#define MAX_CMDS               16  ///< 最多支持的命令数

/** 单个命令的回调列表 */
typedef struct {
    uint32_t        cmd_id;
    cmd_callback_t  cbs[MAX_CALLBACKS_PER_CMD];
    void           *args[MAX_CALLBACKS_PER_CMD];
    int             count;
} cmd_entry_t;

static cmd_entry_t s_cmd_table[MAX_CMDS];
static int         s_cmd_count = 0;

/* ------------------------------------------------------------------ */
/* 内部工具函数                                                       */
/* ------------------------------------------------------------------ */

/** 按命令 ID 查找条目，返回索引；未找到返回 -1 */
static int find_cmd_entry(uint32_t cmd_id)
{
    for (int i = 0; i < s_cmd_count; i++) {
        if (s_cmd_table[i].cmd_id == cmd_id)
            return i;
    }
    return -1;
}

/** 获取或创建命令条目 */
static int get_or_create_cmd(uint32_t cmd_id)
{
    int idx = find_cmd_entry(cmd_id);
    if (idx >= 0)
        return idx;

    if (s_cmd_count >= MAX_CMDS)
        return -1;

    idx = s_cmd_count++;
    s_cmd_table[idx].cmd_id = cmd_id;
    s_cmd_table[idx].count  = 0;
    return idx;
}

/* ------------------------------------------------------------------ */
/* 默认命令实现                                                       */
/* ------------------------------------------------------------------ */

/** 命令：循环切换背光亮度（0% → 50% → 100% → 0%） */
static void cmd_backlight_toggle(void *arg)
{
    (void)arg;
    static uint8_t level = 50; /* 初始 50% */

    if (level == 0)
        level = 50;
    else if (level == 50)
        level = 100;
    else
        level = 0;

    struct Device *bl = device_find("backlight");
    if (bl && bl->ops && bl->ops->write) {
        bl->ops->write(bl->data, &level, sizeof(level));
        ESP_LOGI(TAG, "背光亮度设置为 %d%%", level);
    } else {
        ESP_LOGE(TAG, "背光设备未找到");
    }
}

/** 命令：打印传感器信息（温度、电池、运行时间） */
static void cmd_show_sensor_info(void *arg)
{
    (void)arg;

    struct Device *temp   = device_find("internal_temp");
    struct Device *bat    = device_find("battery");
    struct Device *uptime = device_find("sys_uptime");

    float temperature = 0.0f;
    if (temp && temp->ops && temp->ops->read)
        temp->ops->read(temp->data, &temperature, sizeof(temperature));

    struct Battery bat_data = {0};
    if (bat && bat->ops && bat->ops->read)
        bat->ops->read(bat->data, &bat_data, sizeof(bat_data));

    uint32_t uptime_sec = 0;
    if (uptime && uptime->ops && uptime->ops->read)
        uptime->ops->read(uptime->data, &uptime_sec, sizeof(uptime_sec));

    ESP_LOGI(TAG, "=== 传感器信息 ===");
    ESP_LOGI(TAG, "  温度:    %.1f °C", temperature);
    ESP_LOGI(TAG, "  电池:    %d mV (%d%%)", bat_data.voltage_mv, bat_data.percent);
    ESP_LOGI(TAG, "  运行时间: %lu 秒", (unsigned long)uptime_sec);
}

/* ------------------------------------------------------------------ */
/* 命令处理任务                                                       */
/* ------------------------------------------------------------------ */

static void cmd_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "命令处理器任务启动");

    while (1) {
        int key_id = 0;

        /* 阻塞等待按键消息 */
        if (xQueueReceive(key_queue, &key_id, portMAX_DELAY) != pdTRUE)
            continue;

        ESP_LOGD(TAG, "收到按键消息: key_id=%d", key_id);

        /* key_id → cmd_id 映射 */
        uint32_t cmd_id;
        if (key_id == 0)
            cmd_id = CMD_KEY_A_SHORT;
        else if (key_id == 1)
            cmd_id = CMD_KEY_B_SHORT;
        else
            continue; /* 未知按键，忽略 */

        /* 查找并执行所有回调 */
        int idx = find_cmd_entry(cmd_id);
        if (idx < 0) {
            ESP_LOGW(TAG, "未注册的命令 ID: %lu", (unsigned long)cmd_id);
            continue;
        }

        for (int i = 0; i < s_cmd_table[idx].count; i++) {
            cmd_callback_t cb = s_cmd_table[idx].cbs[i];
            void *cb_arg      = s_cmd_table[idx].args[i];
            if (cb)
                cb(cb_arg);
        }
    }
}

/* ------------------------------------------------------------------ */
/* 公开 API                                                           */
/* ------------------------------------------------------------------ */

int command_handler_init(void)
{
    /* 注册默认命令 */
    command_handler_register(CMD_KEY_A_SHORT, cmd_backlight_toggle, NULL);
    command_handler_register(CMD_KEY_B_SHORT, cmd_show_sensor_info, NULL);

    /* 创建命令处理任务 */
    BaseType_t ret = xTaskCreate(cmd_task, "cmd_handler", 4096, NULL, 4, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建命令处理任务失败");
        return -1;
    }

    ESP_LOGI(TAG, "命令处理器初始化完成（key_a→背光, key_b→传感器信息）");
    return 0;
}

int command_handler_register(uint32_t cmd_id, cmd_callback_t cb, void *arg)
{
    if (cb == NULL)
        return -1;

    int idx = get_or_create_cmd(cmd_id);
    if (idx < 0) {
        ESP_LOGE(TAG, "命令表已满");
        return -1;
    }

    if (s_cmd_table[idx].count >= MAX_CALLBACKS_PER_CMD) {
        ESP_LOGE(TAG, "命令 %lu 的回调数已达上限", (unsigned long)cmd_id);
        return -1;
    }

    s_cmd_table[idx].cbs[s_cmd_table[idx].count]  = cb;
    s_cmd_table[idx].args[s_cmd_table[idx].count] = arg;
    s_cmd_table[idx].count++;

    return 0;
}

int command_handler_unregister(uint32_t cmd_id, cmd_callback_t cb)
{
    int idx = find_cmd_entry(cmd_id);
    if (idx < 0)
        return -1;

    for (int i = 0; i < s_cmd_table[idx].count; i++) {
        if (s_cmd_table[idx].cbs[i] == cb) {
            /* 用最后一个元素覆盖当前位置，保持紧凑 */
            int last = s_cmd_table[idx].count - 1;
            s_cmd_table[idx].cbs[i]  = s_cmd_table[idx].cbs[last];
            s_cmd_table[idx].args[i] = s_cmd_table[idx].args[last];
            s_cmd_table[idx].count--;
            return 0;
        }
    }

    return -1; /* 未找到该回调 */
}
