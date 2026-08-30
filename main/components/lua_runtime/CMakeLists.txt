

idf_component_register(
    SRCS
        "lua_runtime.c"
        "lua_device.c"
    INCLUDE_DIRS "."
    REQUIRES
        lua
        framework
)
