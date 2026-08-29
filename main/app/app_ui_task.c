/**
 * @file app_ui_task.c
 * @brief UI 显示任务实现
 *
 * 显示内容：
 *   - 标题：Sensor Dashboard
 *   - 温度（内部温度传感器）
 *   - 电池（电压 + 电量百分比）
 *   - 运行时间（秒）
 *
 * 更新频率：每 2 秒刷新一次数据。
 */
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

/** UI 更新间隔（毫秒） */
#define UI_UPDATE_INTERVAL_MS  2000

/* UI 对象句柄 */
static lv_obj_t *s_temp_label;
static lv_obj_t *s_battery_label;
static lv_obj_t *s_uptime_label;

/* ------------------------------------------------------------------ */
/* UI 构建                                                            */
/* ------------------------------------------------------------------ */

static void create_ui(void)
{
    lvgl_port_lock(0);

    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);

    /* 标题 */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Sensor Dashboard");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    /* 温度标签 */
    s_temp_label = lv_label_create(scr);
    lv_label_set_text(s_temp_label, "Temperature: -- °C");
    lv_obj_set_style_text_color(s_temp_label, lv_color_hex(0xFF8800), 0);
    lv_obj_align(s_temp_label, LV_ALIGN_TOP_MID, 0, 50);

    /* 电池标签 */
    s_battery_label = lv_label_create(scr);
    lv_label_set_text(s_battery_label, "Battery: -- mV (--%)");
    lv_obj_set_style_text_color(s_battery_label, lv_color_hex(0x00FF00), 0);
    lv_obj_align(s_battery_label, LV_ALIGN_TOP_MID, 0, 90);

    /* 运行时间标签 */
    s_uptime_label = lv_label_create(scr);
    lv_label_set_text(s_uptime_label, "Uptime: -- s");
    lv_obj_set_style_text_color(s_uptime_label, lv_color_hex(0x00CCFF), 0);
    lv_obj_align(s_uptime_label, LV_ALIGN_TOP_MID, 0, 130);

    lvgl_port_unlock();
}

/* ------------------------------------------------------------------ */
/* UI 数据更新                                                        */
/* ------------------------------------------------------------------ */

static void update_ui(void)
{
    /* 获取设备 */
    struct Device *temp_dev   = device_find("internal_temp");
    struct Device *bat_dev    = device_find("battery");
    struct Device *uptime_dev = device_find("sys_uptime");

    /* 读取数据 */
    float temp = 0.0f;
    if (temp_dev && temp_dev->ops && temp_dev->ops->read)
        temp_dev->ops->read(temp_dev->data, &temp, sizeof(temp));

    struct Battery bat = { 0 };
    if (bat_dev && bat_dev->ops && bat_dev->ops->read)
        bat_dev->ops->read(bat_dev->data, &bat, sizeof(bat));

    uint32_t uptime_sec = 0;
    if (uptime_dev && uptime_dev->ops && uptime_dev->ops->read)
        uptime_dev->ops->read(uptime_dev->data, &uptime_sec, sizeof(uptime_sec));

    /* 更新 LVGL 标签（必须加锁） */
    lvgl_port_lock(0);

    if (s_temp_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Temperature: %.1f °C", temp);
        lv_label_set_text(s_temp_label, buf);
    }
    if (s_battery_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Battery: %d mV (%d%%)",
                 bat.voltage_mv, bat.percent);
        lv_label_set_text(s_battery_label, buf);
    }
    if (s_uptime_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Uptime: %lu s", (unsigned long)uptime_sec);
        lv_label_set_text(s_uptime_label, buf);
    }

    lvgl_port_unlock();
}

/* ------------------------------------------------------------------ */
/* 任务主函数                                                         */
/* ------------------------------------------------------------------ */

void ui_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "UI 任务启动");

    /* 1. 创建初始 UI */
    create_ui();

    /* 2. 主循环：周期性刷新数据 */
    while (1) {
        update_ui();
        vTaskDelay(pdMS_TO_TICKS(UI_UPDATE_INTERVAL_MS));
    }
}
