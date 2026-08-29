/**
 * @file internal_temp.c
 * @brief 内部温度传感器驱动实现
 */
#include "internal_temp.h"
#include "../framework/framework.h"

#include "driver/temperature_sensor.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "internal_temp";

/** 温度量程配置 */
#define TEMP_RANGE_MIN  -10  /* °C */
#define TEMP_RANGE_MAX   80  /* °C */

static struct InternalTemp        g_temp;
static temperature_sensor_handle_t g_temp_handle = NULL;

/* ------------------------------------------------------------------ */
/* 设备操作函数                                                       */
/* ------------------------------------------------------------------ */

static int internal_temp_init(void *self)
{
    struct InternalTemp *temp = (struct InternalTemp *)self;

    /* 1. 配置温度传感器 */
    temperature_sensor_config_t config =
        TEMPERATURE_SENSOR_CONFIG_DEFAULT(TEMP_RANGE_MIN, TEMP_RANGE_MAX);

    /* 2. 安装传感器 */
    esp_err_t ret = temperature_sensor_install(&config, &g_temp_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "温度传感器安装失败: %s", esp_err_to_name(ret));
        return -1;
    }

    /* 3. 启用 */
    ret = temperature_sensor_enable(g_temp_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "温度传感器启用失败: %s", esp_err_to_name(ret));
        return -1;
    }

    temp->temperature = 0.0f;

    ESP_LOGI(TAG, "内部温度传感器初始化完成（量程 %d~%d°C）",
             TEMP_RANGE_MIN, TEMP_RANGE_MAX);
    return 0;
}

static int internal_temp_read(void *self, void *buf, size_t len)
{
    struct InternalTemp *temp = (struct InternalTemp *)self;

    if (len < sizeof(float))
        return -1;

    float celsius = 0.0f;
    esp_err_t ret = temperature_sensor_get_celsius(g_temp_handle, &celsius);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取温度失败: %s", esp_err_to_name(ret));
        return -1;
    }

    temp->temperature = celsius;
    memcpy(buf, &temp->temperature, sizeof(float));
    return sizeof(float);
}

static int internal_temp_write(void *self, const void *buf, size_t len)
{
    (void)self; (void)buf; (void)len;
    return -1; /* 只读设备 */
}

static int internal_temp_deinit(void *self)
{
    (void)self;

    if (g_temp_handle != NULL) {
        temperature_sensor_disable(g_temp_handle);
        temperature_sensor_uninstall(g_temp_handle);
        g_temp_handle = NULL;
    }
    return 0;
}

static const struct DeviceOps internal_temp_ops = {
    .init   = internal_temp_init,
    .read   = internal_temp_read,
    .write  = internal_temp_write,
    .deinit = internal_temp_deinit,
};

static struct Device g_internal_temp_device = {
    .name = "internal_temp",
    .data = &g_temp,
    .ops  = &internal_temp_ops,
};

struct Device *InternalTemp_get_device(void)
{
    return &g_internal_temp_device;
}
