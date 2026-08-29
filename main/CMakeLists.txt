idf_component_register(
    SRCS
        "main.c"
        "framework/framework.c"
        "framework/array.c"
        "drivers/sensor.c"
        "drivers/led.c"
        "drivers/key.c"
        "app/app_temp_task.c"
        "app/app_key_task.c"
        "app/app_ui_task.c"
        "drivers/internal_temp.c"
        "drivers/sys_uptime.c"
        "drivers/battery.c"
        "drivers/display.c"
        "drivers/backlight.c"
        "drivers/uart_ph2.c"
        // "drivers/sd_card_spi.c"  // SPI 模式（兼容性更好，WS2812 干扰下可用）
        "drivers/sd_card.c"    // SDMMC 模式（速度更快，需信号质量好）
        "drivers/ws2812b.c"    // WS2812B RGB LED (GPIO41)
        "app/app_led_task.c"
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
