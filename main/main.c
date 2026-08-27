#include <stdio.h>

#include "framework/framework.h"
#include "drivers/sensor.h"
#include "drivers/led.h"
#include "drivers/ws2812b.h"
#include "esp_log.h"

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

    // 设置红色：RGB = 255, 0, 0
    uint8_t red_color[3] = {255, 0, 0};
    ws2812b->ops->write(ws2812b->data, red_color, sizeof(red_color));

    // 读回当前颜色
    uint8_t current_color[3] = {0};
    ws2812b->ops->read(ws2812b->data, current_color, sizeof(current_color));

    ESP_LOGI("app", "WS2812B 颜色：R=%d G=%d B=%d",
             current_color[0], current_color[1], current_color[2]);
}