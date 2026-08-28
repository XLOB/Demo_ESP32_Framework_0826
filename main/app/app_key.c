#include "app/app_key.h"

#include "framework/framework.h"
#include "drivers/key.h"
#include "drivers/ws2812b.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "app_key";

void key_task(void *arg)
{
    // 从框架获取设备
    struct Device *key_dev = Key_get_device();
    struct Device *ws2812b_dev = Ws2812b_get_device();

    // 初始化设备
    if (key_dev && key_dev->ops && key_dev->ops->init)
    {
        key_dev->ops->init(key_dev->data);
    }

    if (ws2812b_dev && ws2812b_dev->ops && ws2812b_dev->ops->init)
    {
        ws2812b_dev->ops->init(ws2812b_dev->data);
    }

    int key_last_state = 1;
    int key_pressed = 0;

    int color_index = 0;

    uint8_t red[3] = {255, 0, 0};
    uint8_t green[3] = {0, 255, 0};
    uint8_t blue[3] = {0, 0, 255};
    uint8_t off[3] = {0, 0, 0};

    while (1)
    {
        int key_state = 0;

        if (key_dev->ops->read)
        {
            key_dev->ops->read(key_dev->data, &key_state, sizeof(key_state));
        }

        // 按下沿
        if (key_last_state == 1 && key_state == 0)
        {
            key_pressed = 1;
        }

        // 松开沿
        if (key_last_state == 0 && key_state == 1)
        {
            if (key_pressed)
            {
                color_index = (color_index + 1) % 4;

                ESP_LOGI(TAG, "按键切换颜色索引：%d", color_index);

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

                key_pressed = 0;
            }
        }

        key_last_state = key_state;

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}