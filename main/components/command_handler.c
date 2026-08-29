#include "command_handler.h"
#include "framework/framework.h"
#include "drivers/backlight.h"
#include "drivers/display.h"
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

// 默认命令实现（示例）
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

static void cmd_show_sensor_info(void *arg)
{
    // 读取温度、电池、运行时间并显示在屏幕上
    struct Device *temp = device_find("internal_temp");
    struct Device *bat = device_find("battery");
    struct Device *uptime = device_find("sys_uptime");
    struct Device *disp = device_find("display");

    if (!disp)
    {
        ESP_LOGE(TAG, "显示设备未找到");
        return;
    }

    float temperature = 0.0f;
    if (temp && temp->ops && temp->ops->read)
        temp->ops->read(temp->data, &temperature, sizeof(temperature));

    struct Battery bat_data = {0};
    if (bat && bat->ops && bat->ops->read)
        bat->ops->read(bat->data, &bat_data, sizeof(bat_data));

    uint32_t uptime_sec = 0;
    if (uptime && uptime->ops && uptime->ops->read)
        uptime->ops->read(uptime->data, &uptime_sec, sizeof(uptime_sec));

    // 简单清屏并显示文字（暂时用填充色块模拟，后面可扩展字体）
    // 这里我们只打印到日志，实际显示需要字体库，先简化
    ESP_LOGI(TAG, "=== 传感器信息 ===");
    ESP_LOGI(TAG, "温度: %.1f °C", temperature);
    ESP_LOGI(TAG, "电池: %d mV (%d%%)", bat_data.voltage_mv, bat_data.percent);
    ESP_LOGI(TAG, "运行时间: %d 秒", uptime_sec);

    // 在屏幕上显示彩色矩形表示不同数据（示范）
    if (disp && disp->ops && disp->ops->write)
    {
        // 简单显示一个颜色块，表示命令已执行
        uint16_t color = 0x07E0; // 绿色
        // 我们可以调用 display_fill_rect 在屏幕上显示一个矩形
        // 但 display.c 中提供了 display_fill_rect 函数，需要包含头文件
        // 这里我们调用 display_fill_rect（需要外部声明）
        // 因为 display_fill_rect 不是 DeviceOps 的一部分，所以我们要包含 display.h
        // 或者直接使用 ops->write 传递帧缓冲，但比较复杂。
        // 为了简化，我们只打印日志，后面如果需要显示文字，可以扩展 display_manager 组件。
    }
}

// 内部命令处理任务
static void cmd_task(void *arg)
{
    ESP_LOGI(TAG, "命令处理器任务启动");

    while (1)
    {
        int msg = 0;
        if (xQueueReceive(key_queue, &msg, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGD(TAG, "收到按键消息: %d", msg);
            // 根据消息值查找命令 ID（此处直接映射）
            uint32_t cmd_id = 0;
            if (msg == 1)
                cmd_id = CMD_KEY_A_SHORT;
            else if (msg == 2)
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

int command_handler_init(void)
{
    // 1. 注册默认命令
    // key_a: 切换背光
    if (command_handler_register(CMD_KEY_A_SHORT, cmd_backlight_toggle, NULL) != 0)
    {
        ESP_LOGE(TAG, "注册背光命令失败");
    }
    // key_b: 显示传感器信息
    if (command_handler_register(CMD_KEY_B_SHORT, cmd_show_sensor_info, NULL) != 0)
    {
        ESP_LOGE(TAG, "注册传感器信息命令失败");
    }

    // 2. 创建任务（优先级略低于应用任务）
    if (xTaskCreate(cmd_task, "cmd_handler", 2048, NULL, 4, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "创建命令处理任务失败");
        return -1;
    }

    ESP_LOGI(TAG, "命令处理器初始化完成");
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