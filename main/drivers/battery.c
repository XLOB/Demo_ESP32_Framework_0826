#include "battery.h"
#include "../framework/framework.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_log.h"

static const char *TAG = "battery";

static struct Battery g_battery;

#define BATTERY_ADC_UNIT ADC_UNIT_2
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_7 // 根据实际硬件调整
#define BATTERY_DIVIDER_RATIO 2           // 分压比

static int battery_init(void *self)
{
    struct Battery *battery = (struct Battery *)self;

    // 初始化参数
    battery->voltage_divider_ratio = BATTERY_DIVIDER_RATIO;
    battery->voltage_mv = 0;
    battery->percent = 0;

    // 1. 创建 ADC oneshot 单元
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = BATTERY_ADC_UNIT,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_cfg, &battery->adc_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "ADC unit init failed: %s", esp_err_to_name(ret));
        return -1;
    }

    // 2. 配置 ADC 通道
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_oneshot_config_channel(battery->adc_handle, BATTERY_ADC_CHANNEL, &chan_cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "ADC channel config failed: %s", esp_err_to_name(ret));
        return -1;
    }

    // 3. 尝试创建校准句柄
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = BATTERY_ADC_UNIT,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &battery->cali_handle);
    if (ret != ESP_OK)
    {
        battery->cali_handle = NULL;
        ESP_LOGW(TAG, "ADC calibration failed, using raw values");
    }

    ESP_LOGI(TAG, "电池 ADC 初始化完成");
    return 0;
}

static int battery_read(void *self, void *buf, size_t len)
{
    struct Battery *battery = (struct Battery *)self;

    if (len < sizeof(struct Battery))
        return -1;

    int raw = 0;
    esp_err_t ret = adc_oneshot_read(battery->adc_handle, BATTERY_ADC_CHANNEL, &raw);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(ret));
        return -1;
    }

    int voltage_mv = 0;

    if (battery->cali_handle != NULL)
    {
        ret = adc_cali_raw_to_voltage(battery->cali_handle, raw, &voltage_mv);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "ADC calibration conversion failed: %s", esp_err_to_name(ret));
            return -1;
        }
    }
    else
    {
        // 没有校准，手动换算：12dB 衰减，3.3V 参考
        voltage_mv = (raw * 3300) / 4095;
    }

    // 应用分压比
    int battery_mv = voltage_mv * battery->voltage_divider_ratio;

    battery->voltage_mv = battery_mv;

    // 粗略估算电量：3.3V ~ 4.2V
    if (battery_mv >= 4200)
        battery->percent = 100;
    else if (battery_mv <= 3300)
        battery->percent = 0;
    else
        battery->percent = (battery_mv - 3300) * 100 / (4200 - 3300);

    memcpy(buf, battery, sizeof(struct Battery));

    return sizeof(struct Battery);
}

static int battery_write(void *self, const void *buf, size_t len)
{
    return -1;
}

static int battery_deinit(void *self)
{
    struct Battery *battery = (struct Battery *)self;

    if (battery->adc_handle)
    {
        adc_oneshot_del_unit(battery->adc_handle);
        battery->adc_handle = NULL;
    }

    if (battery->cali_handle)
    {
        adc_cali_delete_scheme_curve_fitting(battery->cali_handle);
        battery->cali_handle = NULL;
    }

    return 0;
}

static const struct DeviceOps battery_ops = {
    .init = battery_init,
    .read = battery_read,
    .write = battery_write,
    .deinit = battery_deinit,
};

static struct Device g_battery_device = {
    .name = "battery",
    .data = &g_battery,
    .ops = &battery_ops,
};

struct Device *Battery_get_device(void)
{
    return &g_battery_device;
}