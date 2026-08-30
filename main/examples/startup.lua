-- startup.lua — 启动脚本
-- 把此文件放到 SD 卡根目录，系统启动时 CLI 会自动执行。
-- 用 Lua 定义自定义命令，这些命令会出现在 help 列表里。

print("[startup.lua] Registering custom commands...")

-- === 命令 1: hello ===
cli.register("hello", function(args)
    local name = args[1] or "world"
    print("Hello, " .. name .. "!")
end, "Say hello [name]")

-- === 命令 2: blink ===
cli.register("blink", function(args)
    local times = tonumber(args[1]) or 3
    local delay_ms = tonumber(args[2]) or 200

    local led = device.find("ws2812b")
    if not led then
        print("Error: ws2812b device not found")
        return -1
    end

    print("Blinking " .. times .. " times...")
    for i = 1, times do
        led:write(string.char(255, 255, 255))
        sys.delay(delay_ms)
        led:write(string.char(0, 0, 0))
        sys.delay(delay_ms)
    end
    print("Done.")
end, "Blink LED [times] [delay_ms]")

-- === 命令 3: temp ===
cli.register("temp", function(args)
    local dev = device.find("internal_temp")
    if not dev then
        print("Internal temp sensor not found")
        return -1
    end
    local data = dev:read(4)
    if data then
        -- 温度数据是 float 格式，4 字节
        -- 简单方式：按字节打印
        print("Raw temp data (" .. #data .. " bytes):")
        local bytes = {}
        for i = 1, #data do
            bytes[i] = string.byte(data, i)
        end
        print("  " .. table.concat(bytes, " "))
    else
        print("Read failed")
    end
end, "Read internal temperature")

-- === 命令 4: rainbow ===
cli.register("rainbow", function(args)
    local steps = tonumber(args[1]) or 20
    local delay_ms = tonumber(args[2]) or 50

    local led = device.find("ws2812b")
    if not led then
        print("Error: ws2812b device not found")
        return -1
    end

    print("Rainbow effect (" .. steps .. " steps)...")

    local function hsv_to_rgb(h, s, v)
        -- h: 0-360, s: 0-1, v: 0-1
        local c = v * s
        local x = c * (1 - math.abs((h / 60) % 2 - 1))
        local m = v - c
        local r, g, b = 0, 0, 0
        if h < 60 then r, g, b = c, x, 0
        elseif h < 120 then r, g, b = x, c, 0
        elseif h < 180 then r, g, b = 0, c, x
        elseif h < 240 then r, g, b = 0, x, c
        elseif h < 300 then r, g, b = x, 0, c
        else r, g, b = c, 0, x
        end
        return math.floor((r + m) * 255),
               math.floor((g + m) * 255),
               math.floor((b + m) * 255)
    end

    for i = 0, steps - 1 do
        local hue = (i / steps) * 360
        local r, g, b = hsv_to_rgb(hue, 1.0, 1.0)
        led:write(string.char(r, g, b))
        sys.delay(delay_ms)
    end

    led:write(string.char(0, 0, 0))
    print("Done.")
end, "Rainbow LED effect [steps] [delay_ms]")

print("[startup.lua] Done! Type 'help' to see new commands.")
