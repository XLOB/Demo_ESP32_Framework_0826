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

// 外部队列（在 main.c 中定义）
extern QueueHandle_t key_queue;

// 命令回调表
typedef struct
{
    uint32_t cmd_id;
    cmd_callback_t cb;
    void *arg;
} cmd_entry_t;

#define MAX_CMDS 16
static cmd_entry_t s_cmd_table[MAX_CMDS];
static int s_cmd_count = 0;

// ===== 默认命令实现 =====

// 命令：循环切换背光亮度（0% -> 50% -> 100% -> 0%）
static void cmd_backlight_toggle(void *arg)
{
    static uint8_t level = 50; // 初始 50%
    // 循环切换：0% -> 50% -> 100% -> 0%
    if (level == 0)
        level = 50;
    else if (level == 50)
        level = 100;
    else
        level = 0;

    struct Device *bl = device_find("backlight");
    if (bl && bl->ops && bl->ops->write)
    {
        bl->ops->write(bl->data, &level, sizeof(level));
        ESP_LOGI(TAG, "背光亮度设置为 %d%%", level);
    }
    else
    {
        ESP_LOGE(TAG, "背光设备未找到");
    }
}

// 命令：打印传感器信息到日志（温度、电池、运行时间）
// 绑定到 key_b
static void cmd_show_sensor_info(void *arg)
{
    struct Device *temp = device_find("internal_temp");
    struct Device *bat = device_find("battery");
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
    ESP_LOGI(TAG, "温度: %.1f °C", temperature);
    ESP_LOGI(TAG, "电池: %d mV (%d%%)", bat_data.voltage_mv, bat_data.percent);
    ESP_LOGI(TAG, "运行时间: %lu 秒", (unsigned long)uptime_sec);
}

// ===== 内部命令处理任务 =====

static void cmd_task(void *arg)
{
    ESP_LOGI(TAG, "命令处理器任务启动");

    while (1)
    {
        int msg = 0;
        if (xQueueReceive(key_queue, &msg, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGD(TAG, "收到按键消息: %d", msg);

            // app_key_task 发送 key_id: 0=key_a, 1=key_b
            uint32_t cmd_id = 0;
            if (msg == 0)
                cmd_id = CMD_KEY_A_SHORT;
            else if (msg == 1)
                cmd_id = CMD_KEY_B_SHORT;
            else
                continue;

            // 查找命令回调
            cmd_callback_t cb = NULL;
            void *cb_arg = NULL;
            for (int i = 0; i < s_cmd_count; i++)
            {
                if (s_cmd_table[i].cmd_id == cmd_id)
                {
                    cb = s_cmd_table[i].cb;
                    cb_arg = s_cmd_table[i].arg;
                    break;
                }
            }
            if (cb)
            {
                cb(cb_arg);
            }
            else
            {
                ESP_LOGW(TAG, "未注册的命令 ID: %lu", cmd_id);
            }
        }
    }
}

// ===== 公开接口 =====

int command_handler_init(void)
{
    // 1. 注册默认命令
    // key_a: 切换背光亮度
    if (command_handler_register(CMD_KEY_A_SHORT, cmd_backlight_toggle, NULL) != 0)
    {
        ESP_LOGE(TAG, "注册背光命令失败");
    }
    // key_b: 显示传感器信息
    if (command_handler_register(CMD_KEY_B_SHORT, cmd_show_sensor_info, NULL) != 0)
    {
        ESP_LOGE(TAG, "注册传感器信息命令失败");
    }

    // 2. 创建任务（优先级略低于 key_task）
    if (xTaskCreate(cmd_task, "cmd_handler", 4096, NULL, 4, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "创建命令处理任务失败");
        return -1;
    }

    ESP_LOGI(TAG, "命令处理器初始化完成（key_a→背光, key_b→传感器信息）");
    return 0;
}

int command_handler_register(uint32_t cmd_id, cmd_callback_t cb, void *arg)
{
    if (!cb)
        return -1;
    if (s_cmd_count >= MAX_CMDS)
    {
        ESP_LOGE(TAG, "命令表已满");
        return -1;
    }

    // 检查是否已存在，存在则覆盖
    for (int i = 0; i < s_cmd_count; i++)
    {
        if (s_cmd_table[i].cmd_id == cmd_id)
        {
            s_cmd_table[i].cb = cb;
            s_cmd_table[i].arg = arg;
            return 0;
        }
    }

    // 新增
    s_cmd_table[s_cmd_count].cmd_id = cmd_id;
    s_cmd_table[s_cmd_count].cb = cb;
    s_cmd_table[s_cmd_count].arg = arg;
    s_cmd_count++;
    return 0;
}

int command_handler_unregister(uint32_t cmd_id)
{
    for (int i = 0; i < s_cmd_count; i++)
    {
        if (s_cmd_table[i].cmd_id == cmd_id)
        {
            // 用最后一个元素覆盖当前
            s_cmd_table[i] = s_cmd_table[s_cmd_count - 1];
            s_cmd_count--;
            return 0;
        }
    }
    return -1; // 未找到
}
