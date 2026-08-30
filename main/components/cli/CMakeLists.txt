idf_component_register(
    SRCS
        "cli.c"
        "lua_cli.c"
    INCLUDE_DIRS "."
    REQUIRES
        lua_runtime
        lua
        framework
        fatfs
        console
)
