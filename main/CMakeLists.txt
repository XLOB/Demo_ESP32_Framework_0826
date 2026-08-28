idf_component_register(
    SRCS
        "main.c"
        "framework/framework.c"
        "drivers/sensor.c"
        "drivers/led.c"
        "drivers/ws2812b.c"
        "drivers/key.c"
        "framework/array.c"
    INCLUDE_DIRS "."
)