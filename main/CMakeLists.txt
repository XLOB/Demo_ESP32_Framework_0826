idf_component_register(
    SRCS
        # 入口
        "main.c"

        # 框架层
        "framework/framework.c"
        "framework/array.c"

        # 驱动层
        "drivers/key.c"
        "drivers/internal_temp.c"
        "drivers/battery.c"
        "drivers/sys_uptime.c"
        "drivers/display.c"
        "drivers/backlight.c"
        "drivers/uart_ph2.c"
        "drivers/sensor.c"
        "drivers/sd_card.c"
        "drivers/ws2812b.c"

        # 应用层
        "app/app_key_task.c"
        "app/app_temp_task.c"
        "app/app_ui_task.c"

        # 组件层
        "components/command_handler.c"

    REQUIRES
        esp_lcd
        esp_driver_gpio
        esp_driver_tsens
        esp_adc
        driver
        esp_timer
        esp_driver_ledc
        esp_driver_uart
        esp_driver_spi
        esp_lvgl_port
        sdmmc
        fatfs
        led_strip

    INCLUDE_DIRS "."
)
