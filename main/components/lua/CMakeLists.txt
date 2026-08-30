set(srcs
    lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c
    lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c
    ltm.c lundump.c lvm.c lzio.c
    lauxlib.c lbaselib.c lcorolib.c ldblib.c liolib.c lmathlib.c
    loadlib.c loslib.c lstrlib.c ltablib.c lutf8lib.c linit.c
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
    REQUIRES
)

# 不要在此处定义 LUA_USE_C89！
#
# 原因：LUA_USE_C89 会触发 luaconf.h 中的 LUA_C89_NUMBERS=1，
# 将 lua_Integer 设为 long（ESP32 上 4 字节）。
# 而 lua_runtime 组件没有此定义，会使用默认的 long long（8 字节），
# 导致 LUAL_NUMSIZES 不匹配 → "incompatible numeric types" panic。
#
# 两个组件必须使用相同的数值类型，所以不定义 LUA_USE_C89，
# 统一使用 Lua 默认的 64 位整数 + double 浮点。

# 去掉一些不需要的警告
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-unused-function
)
