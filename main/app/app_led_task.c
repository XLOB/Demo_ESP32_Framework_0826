#include "app/app_led_task.h"

#include "framework/framework.h"
#include "drivers/ws2812b.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "app_led";

extern QueueHandle_t key_queue;

typedef struct
{
    int key_id;
} key_msg_t;

void led_task(void *arg)
{
    // 通过设备框架获取 ws2812b 设备
    struct Device *ws2812b_dev = device_find("ws2812b");
    if (ws2812b_dev == NULL)
    {
        ESP_LOGE(TAG, "未找到 ws2812b 设备");
        vTaskDelete(NULL);
        return;
    }

    int color_index = 0;

    uint8_t red[3] = {255, 0, 0};
    uint8_t green[3] = {0, 255, 0};
    uint8_t blue[3] = {0, 0, 255};
    uint8_t off[3] = {0, 0, 0};

    while (1)
    {
        key_msg_t msg;
        // 等待按键消息
        if (xQueueReceive(key_queue, &msg, portMAX_DELAY) == pdTRUE)
        {
            color_index = (color_index + 1) % 4;

            ESP_LOGI(TAG, "LED 切换颜色索引：%d (key_id=%d)", color_index, msg.key_id);

            int ret = -1;
            if (color_index == 0)
            {
                ret = ws2812b_dev->ops->write(ws2812b_dev->data, red, sizeof(red));
            }
            else if (color_index == 1)
            {
                ret = ws2812b_dev->ops->write(ws2812b_dev->data, green, sizeof(green));
            }
            else if (color_index == 2)
            {
                ret = ws2812b_dev->ops->write(ws2812b_dev->data, blue, sizeof(blue));
            }
            else
            {
                ret = ws2812b_dev->ops->write(ws2812b_dev->data, off, sizeof(off));
            }

            if (ret < 0)
            {
                ESP_LOGE(TAG, "LED 写入失败");
            }
        }
    }
}