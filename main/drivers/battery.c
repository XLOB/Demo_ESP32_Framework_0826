#include "battery.h"
#include "../framework/framework.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_log.h"

static const char *TAG = "battery";

// ADC 句柄和校准句柄
static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t cali_handle = NULL;

static struct Battery g_battery;

#define BATTERY_ADC_UNIT ADC_UNIT_2
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_7 // 需要根据 GPIO18 实际对应调整

static int battery_init(void *self)
{
    struct Battery *battery = (struct Battery *)self;

    // 1. 创建 ADC oneshot 单元
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = BATTERY_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc1_handle));

    // 2. 配置 ADC 通道
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, BATTERY_ADC_CHANNEL, &chan_cfg));

    // 3. 尝试创建校准句柄
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = BATTERY_ADC_UNIT,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle) != ESP_OK)
    {
        cali_handle = NULL;
        ESP_LOGW(TAG, "ADC 校准失败，将使用原始值");
    }

    battery->voltage_mv = 0;
    battery->percent = 0;

    ESP_LOGI(TAG, "电池 ADC 初始化完成");

    return 0;
}

static int battery_read(void *self, void *buf, size_t len)
{
    struct Battery *battery = (struct Battery *)self;

    if (len < sizeof(struct Battery))
        return -1;

    int raw = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, BATTERY_ADC_CHANNEL, &raw));

    int voltage_mv = 0;

    if (cali_handle != NULL)
    {
        adc_cali_raw_to_voltage(cali_handle, raw, &voltage_mv);
    }
    else
    {
        // 没有校准，手动换算：12dB 衰减，3.3V 参考
        voltage_mv = (raw * 3300) / 4095;
    }

    // 假设分压比是 2:1，也就是电池电压是 ADC 电压的 2 倍
    int battery_mv = voltage_mv * 2;

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
    if (adc1_handle)
    {
        adc_oneshot_del_unit(adc1_handle);
        adc1_handle = NULL;
    }

    if (cali_handle)
    {
        adc_cali_delete_scheme_curve_fitting(cali_handle);
        cali_handle = NULL;
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