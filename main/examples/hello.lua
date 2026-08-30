-- hello.lua — Lua 脚本示例
-- 烧录固件后，把此文件放到 SD 卡根目录，
-- 在串口命令行输入：run hello.lua

print("=== Hello from Lua! ===")

-- 测试 sys 模块
print("Free memory: " .. sys.meminfo() .. " bytes")
print("System tick: " .. sys.tick_ms() .. " ms")

-- 测试 device 模块：列出所有设备
print("\nDevices:")
local devs = device.list()
for i, name in ipairs(devs) do
    print("  " .. i .. ". " .. name)
end

-- 测试读取内部温度传感器
print("\nReading internal temperature...")
local temp_dev = device.find("internal_temp")
if temp_dev then
    local data = temp_dev:read(4)
    if data then
        -- 温度数据是 float，4 字节，这里简单打印原始值
        print("  Read " .. #data .. " bytes from internal_temp")
        print("  Device name: " .. temp_dev.name)
    else
        print("  Read failed")
    end
else
    print("  Device not found")
end

-- 测试 LED 颜色循环
print("\nBlinking LED (R-G-B)...")
local led = device.find("ws2812b")
if led then
    -- 红色
    led:write(string.char(255, 0, 0))
    sys.delay(500)
    -- 绿色
    led:write(string.char(0, 255, 0))
    sys.delay(500)
    -- 蓝色
    led:write(string.char(0, 0, 255))
    sys.delay(500)
    -- 灭
    led:write(string.char(0, 0, 0))
    print("  Done!")
else
    print("  ws2812b device not found")
end

print("\n=== Script finished ===")
