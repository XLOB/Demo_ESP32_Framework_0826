#include "framework/framework.h"

#include "drivers/key.h"
#include "drivers/internal_temp.h"
#include "drivers/battery.h"
#include "drivers/sys_uptime.h"
#include "drivers/display.h"
#include "drivers/backlight.h"
#include "drivers/uart_ph2.h"
#include "drivers/led.h"
#include "drivers/sensor.h"

#include "app/app_key_task.h"
#include "app/app_temp_task.h"
#include "app/app_ui_task.h"

#include "components/command_handler.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "main";

QueueHandle_t key_queue;

void app_main(void)
{
    // 1. 注册所有设备
    device_register(Key_create(46, "key_a"));
    device_register(Key_create(45, "key_b"));
    device_register(InternalTemp_get_device());
    device_register(Battery_get_device());
    device_register(SysUptime_get_device());
    device_register(Display_get_device());
    device_register(Backlight_get_device());
    device_register(UartPh2_get_device());
    device_register(Led_get_device());
    device_register(VirtualSensor_get_device());

    // 2. 统一初始化所有设备
    if (device_init_all() != 0)
    {
        ESP_LOGE(TAG, "Device initialization failed");
        return;
    }

    // 3. 创建消息队列（用于按键事件）
    key_queue = xQueueCreate(10, sizeof(int));

    // 4. 初始化命令处理器（内部创建 cmd_task 消费 key_queue，
    //    注册默认命令：key_a→背光切换，key_b→显示传感器信息）
    command_handler_init();

    // 5. 创建应用任务
    xTaskCreate(key_task, "key_task", 4096, NULL, 5, NULL);
    xTaskCreate(temp_task, "temp_task", 4096, NULL, 4, NULL);

    // 创建 UI 任务（优先级可设为 3，低于 key_task 和 temp_task）
    xTaskCreate(ui_task, "ui_task", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "System started");
}
