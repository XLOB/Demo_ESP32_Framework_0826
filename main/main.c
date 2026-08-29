#include "framework/framework.h"
#include "drivers/key.h"
#include "drivers/ws2812b.h"
#include "drivers/internal_temp.h"
#include "drivers/battery.h"
#include "drivers/sys_uptime.h"
#include "drivers/display.h"

#include "app/app_key_task.h"
#include "app/app_led_task.h"
#include "app/app_temp_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

QueueHandle_t key_queue;

void app_main(void)
{
    // 注册设备
    device_register(Key_get_device());
    device_register(Ws2812b_get_device());
    device_register(InternalTemp_get_device());
    device_register(SysUptime_get_device());
    device_register(Battery_get_device());
    device_register(Display_get_device());

    // 创建队列
    key_queue = xQueueCreate(10, sizeof(int));

    // 创建任务
    xTaskCreate(key_task, "key", 4096, NULL, 5, NULL);
    xTaskCreate(led_task, "led", 4096, NULL, 6, NULL);
    xTaskCreate(temp_task, "temp", 4096, NULL, 4, NULL);

    // 简单测试电池驱动
    struct Device *battery = device_find("battery");

    if (battery && battery->ops && battery->ops->init)
        battery->ops->init(battery->data);

    struct Battery info;
    battery->ops->read(battery->data, &info, sizeof(info));

    ESP_LOGI("app", "电池电压：%dmV，电量：%d%%", info.voltage_mv, info.percent);

    ////////////////////////////////////////////////////////////////////////////////

    struct Device *display = device_find("display");
    if (display && display->ops && display->ops->init)
        display->ops->init(display->data);

    display_clear(display, 0x0000);                        // 清屏黑色
    display_fill_rect(display, 10, 10, 100, 100, 0xF800);  // 红色矩形
    display_fill_rect(display, 130, 10, 100, 100, 0x07E0); // 绿色矩形
    display_fill_rect(display, 70, 130, 100, 100, 0x001F); // 蓝色矩形
}