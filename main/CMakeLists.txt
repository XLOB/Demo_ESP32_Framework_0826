idf_component_register(
    SRCS
        "main.c"
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
        # 组件层（非独立组件，直接编译）
        "components/command_handler.c"
    REQUIRES
        framework          # 关键：让 main 依赖 framework 组件
        lua
        lua_runtime
        cli
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
        # led_strip
        espressif__led_strip   
    INCLUDE_DIRS "."
)