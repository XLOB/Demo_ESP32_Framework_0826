#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "framework/framework.h"

#include "drivers/internal_temp.h"

void app_main(void)
{
    // 注册
    device_register(InternalTemp_get_device());

    // 找到设备
    struct Device *dev = device_find("internal_temp");

    if (dev && dev->ops && dev->ops->init)
        dev->ops->init(dev->data);

    float temp = 0.0f;
    dev->ops->read(dev->data, &temp, sizeof(temp));

    ESP_LOGI("app", "内部温度：%.2f °C", temp);

    // 其他任务...
}