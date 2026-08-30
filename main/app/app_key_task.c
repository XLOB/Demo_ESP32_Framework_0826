/**
 * @file app_key_task.c
 * @brief 按键任务实现
 *
 * 轮询周期：20ms（50Hz），与按键驱动的软件消抖配合使用。
 * 检测到按键按下时，通过 key_queue 发送按键 ID。
 */
#include "app/app_key_task.h"

#include "framework/framework.h"
#include "drivers/key.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "app_key";

/** 全局按键消息队列（在 main.c 中定义） */
extern QueueHandle_t key_queue;

/**
 * 按键 ID 定义：
 *   0 = key_a, 1 = key_b
 *
 * 注意：队列元素类型为 int（见 main.c 中 xQueueCreate(10, sizeof(int))），
 * 发送方和接收方必须使用同一类型，不能包装成结构体再发送，
 * 否则当结构体因对齐填充导致 sizeof != sizeof(int) 时会内存越界。
 */

/** 轮询周期（毫秒） */
#define KEY_POLL_PERIOD_MS  20

/* ------------------------------------------------------------------ */
/* 按键回调                                                           */
/* ------------------------------------------------------------------ */

static void on_key_a_pressed(void)
{
    int key_id = 0;
    if (xQueueSend(key_queue, &key_id, 0) == pdTRUE)
        ESP_LOGD(TAG, "key_a 按下，消息已发送");
}

static void on_key_b_pressed(void)
{
    int key_id = 1;
    if (xQueueSend(key_queue, &key_id, 0) == pdTRUE)
        ESP_LOGD(TAG, "key_b 按下，消息已发送");
}

/* ------------------------------------------------------------------ */
/* 任务主函数                                                         */
/* ------------------------------------------------------------------ */

void key_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "key_task 启动");

    /* 查找按键设备 */
    struct Device *key_a = device_find("key_a");
    struct Device *key_b = device_find("key_b");

    if (key_a == NULL || key_b == NULL) {
        ESP_LOGE(TAG, "未找到按键设备");
        vTaskDelete(NULL);
        return;
    }

    /* 注册按键回调（设备已在 device_init_all 中初始化） */
    Key_set_callback(key_a, on_key_a_pressed);
    Key_set_callback(key_b, on_key_b_pressed);

    /* 主循环：周期性轮询 */
    while (1) {
        Key_poll(key_a);
        Key_poll(key_b);
        vTaskDelay(pdMS_TO_TICKS(KEY_POLL_PERIOD_MS));
    }
}
