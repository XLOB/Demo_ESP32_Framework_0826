#include "uart_ph2.h"
#include "../framework/framework.h"

#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "uart_ph2";

#define PH2_UART_NUM UART_NUM_1
#define PH2_TX_GPIO 5
#define PH2_RX_GPIO 6
#define PH2_UART_BUF_SIZE 256

static struct UartPh2 g_uart_ph2;

static int uart_ph2_init(void *self)
{
    struct UartPh2 *uart = (struct UartPh2 *)self;

    uart_config_t uart_cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_driver_install(PH2_UART_NUM, PH2_UART_BUF_SIZE, 0, 0, NULL, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "UART 驱动安装失败");
        return -1;
    }

    ret = uart_param_config(PH2_UART_NUM, &uart_cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "UART 参数配置失败");
        return -1;
    }

    ret = uart_set_pin(PH2_UART_NUM, PH2_TX_GPIO, PH2_RX_GPIO, -1, -1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "UART 引脚配置失败");
        return -1;
    }

    uart->uart_num = PH2_UART_NUM;
    uart->baud_rate = 115200;

    ESP_LOGI(TAG, "PH2.0 UART 初始化完成");
    return 0;
}

static int uart_ph2_write(void *self, const void *buf, size_t len)
{
    struct UartPh2 *uart = (struct UartPh2 *)self;

    if (buf == NULL || len == 0)
        return -1;

    int written = uart_write_bytes(uart->uart_num, buf, len);
    if (written < 0)
        return -1;

    return written;
}

static int uart_ph2_read(void *self, void *buf, size_t len)
{
    struct UartPh2 *uart = (struct UartPh2 *)self;

    if (buf == NULL || len == 0)
        return -1;

    int read_len = uart_read_bytes(uart->uart_num, buf, len, pdMS_TO_TICKS(100));
    if (read_len < 0)
        return -1;

    return read_len;
}

static int uart_ph2_deinit(void *self)
{
    struct UartPh2 *uart = (struct UartPh2 *)self;
    uart_driver_delete(uart->uart_num);
    return 0;
}

static const struct DeviceOps uart_ph2_ops = {
    .init = uart_ph2_init,
    .read = uart_ph2_read,
    .write = uart_ph2_write,
    .deinit = uart_ph2_deinit,
};

static struct Device g_uart_ph2_device = {
    .name = "uart_ph2",
    .data = &g_uart_ph2,
    .ops = &uart_ph2_ops,
};

struct Device *UartPh2_get_device(void)
{
    return &g_uart_ph2_device;
}