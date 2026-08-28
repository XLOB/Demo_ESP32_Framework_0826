#include "app/app_temp.h"

#include "framework/framework.h"
#include "drivers/internal_temp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "app_temp";

void temp_task(void *arg)
{
    // 从框架获取设备
    struct Device *temp_dev = InternalTemp_get_device();

    // 初始化设备
    if (temp_dev && temp_dev->ops && temp_dev->ops->init)
    {
        temp_dev->ops->init(temp_dev->data);
    }

    while (1)
    {
        float temperature = 0.0f;

        if (temp_dev && temp_dev->ops && temp_dev->ops->read)
        {
            temp_dev->ops->read(temp_dev->data, &temperature, sizeof(temperature));
            ESP_LOGI(TAG, "Temperature: %.2f°C", temperature);
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // 延时1秒
    }
}