#include "app/app_key_task.h"

#include "framework/framework.h"
#include "drivers/key.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "app_key";

extern QueueHandle_t key_queue;

// 消息类型：携带按键ID
typedef struct
{
    int key_id; // 0: key_a, 1: key_b
} key_msg_t;

// 按键回调
static void on_key_a_pressed(void)
{
    key_msg_t msg = {.key_id = 0};
    if (xQueueSend(key_queue, &msg, 0) == pdTRUE)
    {
        ESP_LOGI(TAG, "key_a 按下，消息已发送");
    }
}

static void on_key_b_pressed(void)
{
    key_msg_t msg = {.key_id = 1};
    if (xQueueSend(key_queue, &msg, 0) == pdTRUE)
    {
        ESP_LOGI(TAG, "key_b 按下，消息已发送");
    }
}

void key_task(void *arg)
{
    ESP_LOGI(TAG, "key_task 启动");

    struct Device *key_a = device_find("key_a");
    struct Device *key_b = device_find("key_b");

    if (key_a == NULL || key_b == NULL)
    {
        ESP_LOGE(TAG, "未找到按键设备");
        vTaskDelete(NULL);
        return;
    }

    // 设备已经在 device_init_all 中初始化，这里只需注册回调
    Key_set_callback(key_a, on_key_a_pressed);
    Key_set_callback(key_b, on_key_b_pressed);

    while (1)
    {
        Key_poll(key_a);
        Key_poll(key_b);

        vTaskDelay(pdMS_TO_TICKS(20)); // 20ms 轮询周期
    }
}