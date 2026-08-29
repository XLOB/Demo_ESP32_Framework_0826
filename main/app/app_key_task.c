#include "app/app_key_task.h"

#include "framework/framework.h"
#include "drivers/key.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "app_key";

extern QueueHandle_t key_queue;

// 按键事件回调
static void on_key_pressed(void)
{
    int msg = 1;
    if (xQueueSend(key_queue, &msg, 0) == pdTRUE)
    {
        ESP_LOGI(TAG, "按键按下，消息已发送");
    }
}

void key_task(void *arg)
{
    ESP_LOGI(TAG, "key_task 启动");

    struct Device *key_dev = device_find("key_a");
    if (key_dev == NULL || key_dev->ops == NULL || key_dev->ops->init == NULL)
    {
        ESP_LOGE(TAG, "未找到按键设备");
        vTaskDelete(NULL);
        return;
    }

    key_dev->ops->init(key_dev->data);

    // 注册回调
    Key_set_callback(key_dev, on_key_pressed);

    while (1)
    {
        // 驱动内部检测按键，并在事件发生时调用回调
        Key_poll(key_dev);

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}