#include "framework/framework.h"

#include "drivers/key.h"
#include "drivers/ws2812b.h"
#include "drivers/internal_temp.h"
#include "drivers/battery.h"
#include "drivers/sys_uptime.h"
#include "drivers/display.h"
#include "drivers/backlight.h"

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
    // 1. 注册设备
    device_register(Key_create(46, "key_a"));
    device_register(Key_create(45, "key_b"));
    device_register(Ws2812b_get_device());
    device_register(InternalTemp_get_device());
    device_register(Battery_get_device());
    device_register(SysUptime_get_device());
    device_register(Display_get_device());
    device_register(Backlight_get_device());

    // 2. 创建队列
    key_queue = xQueueCreate(10, sizeof(int));

    // 3. 创建任务
    xTaskCreate(key_task, "key", 4096, NULL, 5, NULL);
    xTaskCreate(led_task, "led", 4096, NULL, 6, NULL);
    xTaskCreate(temp_task, "temp", 4096, NULL, 4, NULL);
}