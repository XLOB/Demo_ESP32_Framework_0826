#include "app/app_temp_task.h"

#include "framework/framework.h"
#include "drivers/internal_temp.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "app_temp";

void temp_task(void *arg)
{
    struct Device *temp_dev = InternalTemp_get_device();

    if (temp_dev && temp_dev->ops && temp_dev->ops->init)
        temp_dev->ops->init(temp_dev->data);

    while (1)
    {
        float temp = 0.0f;

        if (temp_dev->ops->read)
            temp_dev->ops->read(temp_dev->data, &temp, sizeof(temp));

        ESP_LOGI(TAG, "内部温度：%.2f °C", temp);

        vTaskDelay(pdMS_TO_TICKS(5000)); // 每 5 秒读一次
    }
}