idf_component_register(
    SRCS
        "cli.c"
        "lua_cli.c"
    INCLUDE_DIRS "."
    REQUIRES
        lua_runtime
        lua
        fatfs
    PRIV_REQUIRES console esp_driver_uart esp_driver_usb_serial_jtag
    PRIV_INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)