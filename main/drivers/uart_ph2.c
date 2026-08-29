/**
 * @file uart_ph2.c
 * @brief PH2.0 接口 UART 驱动实现
 */
#include "uart_ph2.h"
#include "../framework/framework.h"

#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "uart_ph2";

static bool s_initialized = false;

/* ------------------------------------------------------------------ */
/* 设备操作函数                                                       */
/* ------------------------------------------------------------------ */

static int uart_ph2_init(void *self)
{
    (void)self;

    uart_config_t uart_config = {
        .baud_rate  = UART_PH2_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    /* 安装驱动 */
    esp_err_t ret = uart_driver_install(
        UART_PH2_PORT_NUM,
        UART_PH2_BUF_SIZE * 2,  /* rx_buffer_size */
        UART_PH2_BUF_SIZE * 2,  /* tx_buffer_size */
        0,                       /* queue_size */
        NULL,                    /* uart_queue */
        0                        /* intr_alloc_flags */
    );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %s", esp_err_to_name(ret));
        return -1;
    }

    /* 配置参数 */
    ret = uart_param_config(UART_PH2_PORT_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART param config failed: %s", esp_err_to_name(ret));
        return -1;
    }

    /* 设置引脚 */
    ret = uart_set_pin(
        UART_PH2_PORT_NUM,
        UART_PH2_TX_PIN,
        UART_PH2_RX_PIN,
        UART_PIN_NO_CHANGE,   /* RTS */
        UART_PIN_NO_CHANGE    /* CTS */
    );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART set pin failed: %s", esp_err_to_name(ret));
        return -1;
    }

    s_initialized = true;

    ESP_LOGI(TAG, "PH2.0 UART 初始化完成（TX=%d, RX=%d, %d baud）",
             UART_PH2_TX_PIN, UART_PH2_RX_PIN, UART_PH2_BAUD_RATE);
    return 0;
}

static int uart_ph2_write(void *self, const void *buf, size_t len)
{
    (void)self;
    if (!s_initialized || buf == NULL || len == 0)
        return -1;

    int ret = uart_write_bytes(UART_PH2_PORT_NUM, buf, len);
    return ret;
}

static int uart_ph2_read(void *self, void *buf, size_t len)
{
    (void)self;
    if (!s_initialized || buf == NULL || len == 0)
        return -1;

    int ret = uart_read_bytes(UART_PH2_PORT_NUM, buf, len, 0);
    return ret;
}

static int uart_ph2_deinit(void *self)
{
    (void)self;
    if (s_initialized) {
        uart_driver_delete(UART_PH2_PORT_NUM);
        s_initialized = false;
    }
    return 0;
}

static const struct DeviceOps uart_ph2_ops = {
    .init   = uart_ph2_init,
    .read   = uart_ph2_read,
    .write  = uart_ph2_write,
    .deinit = uart_ph2_deinit,
};

static struct Device g_uart_ph2_device = {
    .name = "uart_ph2",
    .data = NULL,
    .ops  = &uart_ph2_ops,
};

/* ------------------------------------------------------------------ */
/* 公开辅助函数                                                       */
/* ------------------------------------------------------------------ */

struct Device *UartPh2_get_device(void)
{
    return &g_uart_ph2_device;
}

int uart_ph2_send(const uint8_t *data, size_t len)
{
    if (!s_initialized || data == NULL || len == 0)
        return -1;

    return uart_write_bytes(UART_PH2_PORT_NUM, data, len);
}

int uart_ph2_recv(uint8_t *buf, size_t len)
{
    if (!s_initialized || buf == NULL || len == 0)
        return -1;

    return uart_read_bytes(UART_PH2_PORT_NUM, buf, len, 0);
}
