#include "app/app_ui_task.h"
#include "framework/framework.h"
#include "drivers/internal_temp.h"
#include "drivers/battery.h"
#include "drivers/sys_uptime.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"

static const char *TAG = "app_ui";

// UI 对象句柄（全局静态，便于更新）
static lv_obj_t *temp_label;
static lv_obj_t *battery_label;
static lv_obj_t *uptime_label;

// 创建 UI 布局
static void create_ui(void)
{
    lvgl_port_lock(0);

    // 清屏
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);

    // 标题
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Sensor Dashboard");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // 温度标签
    temp_label = lv_label_create(scr);
    lv_label_set_text(temp_label, "Temperature: -- °C");
    lv_obj_set_style_text_color(temp_label, lv_color_hex(0xFF8800), 0);
    lv_obj_align(temp_label, LV_ALIGN_TOP_MID, 0, 50);

    // 电池标签
    battery_label = lv_label_create(scr);
    lv_label_set_text(battery_label, "Battery: -- mV (--%)");
    lv_obj_set_style_text_color(battery_label, lv_color_hex(0x00FF00), 0);
    lv_obj_align(battery_label, LV_ALIGN_TOP_MID, 0, 90);

    // 运行时间标签
    uptime_label = lv_label_create(scr);
    lv_label_set_text(uptime_label, "Uptime: -- s");
    lv_obj_set_style_text_color(uptime_label, lv_color_hex(0x00CCFF), 0);
    lv_obj_align(uptime_label, LV_ALIGN_TOP_MID, 0, 130);

    lvgl_port_unlock();
}

// 更新 UI 数据
static void update_ui(void)
{
    // 获取设备
    struct Device *temp_dev = device_find("internal_temp");
    struct Device *bat_dev = device_find("battery");
    struct Device *uptime_dev = device_find("sys_uptime");

    float temp = 0.0f;
    if (temp_dev && temp_dev->ops && temp_dev->ops->read)
    {
        temp_dev->ops->read(temp_dev->data, &temp, sizeof(temp));
    }

    struct Battery bat = {0};
    if (bat_dev && bat_dev->ops && bat_dev->ops->read)
    {
        bat_dev->ops->read(bat_dev->data, &bat, sizeof(bat));
    }

    uint32_t uptime_sec = 0;
    if (uptime_dev && uptime_dev->ops && uptime_dev->ops->read)
    {
        uptime_dev->ops->read(uptime_dev->data, &uptime_sec, sizeof(uptime_sec));
    }

    // 更新 LVGL 标签（必须加锁）
    lvgl_port_lock(0);
    if (temp_label)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "Temperature: %.1f °C", temp);
        lv_label_set_text(temp_label, buf);
    }
    if (battery_label)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "Battery: %d mV (%d%%)", bat.voltage_mv, bat.percent);
        lv_label_set_text(battery_label, buf);
    }
    if (uptime_label)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "Uptime: %lu s", (unsigned long)uptime_sec);
        lv_label_set_text(uptime_label, buf);
    }
    lvgl_port_unlock();
}

// UI 任务主函数
void ui_task(void *arg)
{
    ESP_LOGI(TAG, "UI 任务启动");

    // 1. 创建初始 UI
    create_ui();

    // 2. 主循环：每 2 秒更新一次数据
    while (1)
    {
        update_ui();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}