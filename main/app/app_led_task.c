#include "app/app_led_task.h"

#include "framework/framework.h"
#include "drivers/ws2812b.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "app_led";

extern QueueHandle_t key_queue;

void led_task(void *arg)
{
    struct Device *ws2812b_dev = Ws2812b_get_device();

    if (ws2812b_dev && ws2812b_dev->ops && ws2812b_dev->ops->init)
        ws2812b_dev->ops->init(ws2812b_dev->data);

    int color_index = 0;

    uint8_t red[3] = {255, 0, 0};
    uint8_t green[3] = {0, 255, 0};
    uint8_t blue[3] = {0, 0, 255};
    uint8_t off[3] = {0, 0, 0};

    while (1)
    {
        int msg = 0;

        // 等待按键消息
        if (xQueueReceive(key_queue, &msg, portMAX_DELAY) == pdTRUE)
        {
            color_index = (color_index + 1) % 4;

            ESP_LOGI(TAG, "LED 切换颜色索引：%d", color_index);

            if (color_index == 0)
            {
                ws2812b_dev->ops->write(ws2812b_dev->data, red, sizeof(red));
            }
            else if (color_index == 1)
            {
                ws2812b_dev->ops->write(ws2812b_dev->data, green, sizeof(green));
            }
            else if (color_index == 2)
            {
                ws2812b_dev->ops->write(ws2812b_dev->data, blue, sizeof(blue));
            }
            else
            {
                ws2812b_dev->ops->write(ws2812b_dev->data, off, sizeof(off));
            }
        }
    }
}