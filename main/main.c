#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h> // 提供 vTaskDelay

#include "framework/framework.h"
#include "drivers/sensor.h"
#include "drivers/led.h"
#include "drivers/ws2812b.h"
#include "esp_log.h"
#include "drivers/key.h"

void app_main(void)
{
    // 1. 注册传感器
    struct Device *sensor_dev = VirtualSensor_get_device();
    if (device_register(sensor_dev) != 0)
    {
        ESP_LOGE("app", "传感器注册失败");
        return;
    }

    // 2. 注册虚拟 LED（示例驱动，保留）
    struct Device *led_dev = Led_get_device();
    if (device_register(led_dev) != 0)
    {
        ESP_LOGE("app", "虚拟 LED 注册失败");
        return;
    }

    // 3. 注册真实 WS2812B
    struct Device *ws2812b_dev = Ws2812b_get_device();
    if (device_register(ws2812b_dev) != 0)
    {
        ESP_LOGE("app", "WS2812B 注册失败");
        return;
    }

    // 4. 注册按键设备
    struct Device *key_dev = Key_get_device();
    if (device_register(key_dev) != 0)
    {
        ESP_LOGE("app", "按键注册失败");
        return;
    }

    // 4. 初始化并读取传感器
    struct Device *sensor = device_find("temp_sensor");
    if (sensor == 0)
    {
        ESP_LOGE("app", "未找到传感器");
        return;
    }
    sensor->ops->init(sensor->data);

    int temp = 0;
    sensor->ops->read(sensor->data, &temp, sizeof(temp));
    ESP_LOGI("app", "传感器值：%d", temp);

    // 5. 使用虚拟 LED
    struct Device *led = device_find("led");
    if (led == 0)
    {
        ESP_LOGE("app", "未找到虚拟 LED");
        return;
    }
    led->ops->init(led->data);

    int state = 1;
    led->ops->write(led->data, &state, sizeof(state));

    int read_state = 0;
    led->ops->read(led->data, &read_state, sizeof(read_state));
    ESP_LOGI("app", "虚拟 LED 状态：%d", read_state);

    // 6. 初始化并控制真实 WS2812B
    struct Device *ws2812b = device_find("ws2812b");
    if (ws2812b == 0)
    {
        ESP_LOGE("app", "未找到 WS2812B");
        return;
    }
    ws2812b->ops->init(ws2812b->data);

    // 初始化按键设备
    struct Device *key = device_find("key_a");
    if (key == 0)
    {
        ESP_LOGE("app", "未找到按键");
        return;
    }
    key->ops->init(key->data);

    // 循环读取按键状态，并根据按键状态控制 WS2812B 的颜色`
    int color_index = 0;

    uint8_t red[3] = {255, 0, 0};
    uint8_t green[3] = {0, 255, 0};
    uint8_t blue[3] = {0, 0, 255};
    uint8_t off[3] = {0, 0, 0};

    while (1)
    {
        int key_state = 0;
        key->ops->read(key->data, &key_state, sizeof(key_state));

        if (key_state == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(30));
            key->ops->read(key->data, &key_state, sizeof(key_state));

            if (key_state == 0)
            {
                color_index = (color_index + 1) % 4;

                if (color_index == 0)
                {
                    ws2812b->ops->write(ws2812b->data, red, sizeof(red));
                }
                else if (color_index == 1)
                {
                    ws2812b->ops->write(ws2812b->data, green, sizeof(green));
                }
                else if (color_index == 2)
                {
                    ws2812b->ops->write(ws2812b->data, blue, sizeof(blue));
                }
                else
                {
                    ws2812b->ops->write(ws2812b->data, off, sizeof(off));
                }

                // 等待松开，防止一次按下触发多次
                while (1)
                {
                    int release_state = 0;
                    key->ops->read(key->data, &release_state, sizeof(release_state));
                    if (release_state == 1)
                        break;
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}