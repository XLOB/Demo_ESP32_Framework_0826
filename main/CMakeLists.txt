idf_component_register(
    SRCS
        "main.c"
        "framework/framework.c"
        "drivers/sensor.c"
        "drivers/led.c"
        "drivers/ws2812b.c"
        "drivers/key.c"
        "framework/array.c"
        "app/app_key_task.c"
        "app/app_temp_task.c"
        "drivers/internal_temp.c"
        "app/app_led_task.c"
        "drivers/sys_uptime.c"
        "drivers/battery.c"
        "drivers/display.c"
        "drivers/backlight.c"
        "drivers/uart_ph2.c"
    REQUIRES
        esp_lcd
        esp_driver_gpio           # 提供 gpio.h
        esp_driver_tsens          # 提供 temperature_sensor.h
        esp_adc
        driver
        esp_timer
        esp_driver_ledc
        esp_driver_uart          # 新增
        esp_driver_spi           # 如果尚未添加，也建议加上（用于显示）
    INCLUDE_DIRS "."
)
