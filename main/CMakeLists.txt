idf_component_register(
    SRCS
        "main.c"
        "framework/framework.c"
        "drivers/sensor.c"
        "drivers/led.c"
        "drivers/ws2812b.c"
        "drivers/key.c"
        "framework/array.c"
        "app/app_key.c"
        "app/app_temp.c"
        "drivers/internal_temp.c"
    INCLUDE_DIRS "."
)