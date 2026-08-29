#include "app/app_temp_task.h"

#include "framework/framework.h"
#include "drivers/internal_temp.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "app_temp";

void temp_task(void *arg)
{
    // 设备已在 device_init_all 中初始化，直接获取
    struct Device *temp_dev = InternalTemp_get_device();
    if (temp_dev == NULL)
    {
        ESP_LOGE(TAG, "未找到温度设备");
        vTaskDelete(NULL);
        return;
    }

    while (1)
    {
        float temp = 0.0f;

        if (temp_dev->ops->read)
        {
            int ret = temp_dev->ops->read(temp_dev->data, &temp, sizeof(temp));
            if (ret > 0)
            {
                ESP_LOGI(TAG, "内部温度：%.2f °C", temp);
            }
            else
            {
                ESP_LOGE(TAG, "读取温度失败");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5000)); // 每 5 秒读一次
    }
}