#include "app/app_key_task.h"

#include "framework/framework.h"
#include "drivers/key.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "app_key";

extern QueueHandle_t key_queue;

// 按键回调函数
static void on_key_event(void)
{
    int msg = 1;
    if (xQueueSend(key_queue, &msg, 0) == pdTRUE)
    {
        ESP_LOGI(TAG, "按键事件已发生，消息已发送");
    }
}

void key_task(void *arg)
{
    struct Device *key_dev = Key_get_device();

    if (key_dev && key_dev->ops && key_dev->ops->init)
        key_dev->ops->init(key_dev->data);

    // 注册回调
    Key_set_callback(key_dev, on_key_event);

    while (1)
    {
        Key_poll(key_dev); // 驱动内部检测按键
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}