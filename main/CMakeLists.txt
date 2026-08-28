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
    REQUIRES
        esp_driver_gpio           # 提供 gpio.h
        esp_driver_tsens          # 提供 temperature_sensor.h
        esp_adc
        driver
        esp_timer
    INCLUDE_DIRS "."
)
