# AI-VOX3 设备框架

> 一个面向 ESP32-S3 的简单、易扩展的设备驱动框架。采用统一设备模型，支持动态注册、查找、初始化设备，并提供了常用硬件驱动（显示、背光、电池、温度、按键、UART 等）和示例任务。

[![Platform](https://img.shields.io/badge/Platform-ESP32--S3-blue.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework](https://img.shields.io/badge/Framework-ESP--IDF%20v5.x-green.svg)](https://docs.espressif.com/projects/esp-idf/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Board](https://img.shields.io/badge/Board-AI--VOX3-orange.svg)]()

---

## ✨ 特性

- **统一设备模型** — 所有设备通过 `Device` 结构体描述，实现标准 `init/read/write/deinit` 接口
- **链表管理** — 设备注册后自动加入全局链表，支持按名称查找和遍历
- **线程安全** — 设备链表使用互斥锁保护，可在多任务环境中安全访问
- **简易驱动开发** — 只需实现 `DeviceOps` 并注册，即可被框架统一管理
- **丰富驱动** — 内置显示、背光、电池、按键、温度、WS2812B 等常用驱动
- **示例任务** — 包含按键轮询、温度读取、LED 控制等应用任务，快速上手

---

## 📁 目录结构

```
.
├── main/                           # 主程序
│   ├── CMakeLists.txt              # 构建脚本（含全部源文件）
│   ├── main.c                      # 入口：注册设备、初始化框架、创建任务
│   │
│   ├── framework/                  # 框架核心
│   │   ├── framework.h             # 设备模型与框架 API
│   │   ├── framework.c             # 链表管理、注册、查找实现
│   │   ├── array.h                 # 动态数组容器
│   │   └── array.c                 # 动态数组实现
│   │
│   ├── drivers/                    # 硬件驱动
│   │   ├── backlight.h / .c        # 背光驱动（LEDC PWM）
│   │   ├── battery.h / .c          # 电池电压检测（ADC + 分压）
│   │   ├── display.h / .c          # ST7789 显示屏（SPI）
│   │   ├── internal_temp.h / .c    # 内部温度传感器
│   │   ├── key.h / .c              # 按键驱动（GPIO 轮询）
│   │   ├── led.h / .c              # 虚拟 LED（示例驱动）
│   │   ├── sensor.h / .c           # 虚拟传感器（示例驱动）
│   │   ├── sys_uptime.h / .c       # 系统运行时间
│   │   ├── uart_ph2.h / .c         # UART 接口（PH2.0 连接器）
│   │   └── ws2812b.h / .c          # WS2812B RGB LED（GPIO41，与 SD 卡冲突）
│   │
│   ├── app/                        # 应用任务
│   │   ├── app_key_task.h / .c     # 按键轮询任务
│   │   ├── app_led_task.h / .c     # LED 控制任务（依赖 WS2812B）
│   │   └── app_temp_task.h / .c    # 温度读取任务
│   │
│   └── components/                 # 内部组件
│       └── command_handler.h / .c  # 命令处理组件（预留）
│
├── docs/                           # 文档
│   └── adding_device.md            # 添加新设备教程
│
├── CMakeLists.txt                  # 顶层 CMake
├── sdkconfig.defaults              # 默认配置
├── partitions.csv                  # 分区表
└── README.md                       # 本文件
```

---

## 🚀 快速开始

### 硬件要求

- AI-VOX3 开发板（ESP32-S3-R8）
- USB Type-C 数据线
- ESP-IDF v5.x 开发环境

### 编译与烧录

```bash
# 配置目标芯片
idf.py set-target esp32s3

# 编译
idf.py build

# 烧录（进入下载模式：按住 BOOT + 短按 PWR）
idf.py -p /dev/ttyUSB0 flash

# 查看串口输出
idf.py -p /dev/ttyUSB0 monitor
```

### 运行示例

系统启动后自动初始化所有已注册设备，并创建以下任务：

| 任务 | 功能 | 周期 |
|------|------|------|
| `temp_task` | 读取 ESP32 内部温度并打印日志 | 每 5 秒 |
| `key_task` | 轮询按键 A/B，检测事件并通过队列发送 | 10ms |
| `led_task` | WS2812B 颜色切换（需手动启用） | 按键触发 |

---

## 🧩 如何添加一个新设备

以添加一个 **蜂鸣器驱动** 为例：

### 第一步：创建驱动文件

在 `drivers/` 下创建 `buzzer.h` 和 `buzzer.c`。

**buzzer.h：**

```c
#ifndef BUZZER_H
#define BUZZER_H

#include "../framework/framework.h"

// 获取蜂鸣器设备实例
struct Device *Buzzer_get_device(void);

#endif
```

**buzzer.c：**

```c
#include "buzzer.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "buzzer";

#define BUZZER_PIN  48   // 假设使用 GPIO48

// 设备私有数据
struct Buzzer {
    bool active;
};

static struct Buzzer g_buzzer;

static int buzzer_init(void *self)
{
    struct Buzzer *b = (struct Buzzer *)self;

    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BUZZER_PIN,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cfg);
    gpio_set_level(BUZZER_PIN, 0);

    b->active = false;
    ESP_LOGI(TAG, "蜂鸣器初始化完成 (GPIO%d)", BUZZER_PIN);
    return 0;
}

static int buzzer_write(void *self, const void *buf, size_t len)
{
    struct Buzzer *b = (struct Buzzer *)self;
    if (!buf || len < 1) return -1;

    uint8_t on = *(const uint8_t *)buf;
    gpio_set_level(BUZZER_PIN, on ? 1 : 0);
    b->active = on ? true : false;
    return 1;
}

static int buzzer_read(void *self, void *buf, size_t len)
{
    struct Buzzer *b = (struct Buzzer *)self;
    if (!buf || len < 1) return -1;
    *(uint8_t *)buf = b->active ? 1 : 0;
    return 1;
}

static int buzzer_deinit(void *self)
{
    gpio_reset_pin(BUZZER_PIN);
    return 0;
}

// 设备操作表
static const struct DeviceOps buzzer_ops = {
    .init   = buzzer_init,
    .read   = buzzer_read,
    .write  = buzzer_write,
    .deinit = buzzer_deinit,
};

// 设备实例
static struct Device g_buzzer_device = {
    .name = "buzzer",
    .data = &g_buzzer,
    .ops  = &buzzer_ops,
};

// 对外暴露获取函数
struct Device *Buzzer_get_device(void)
{
    return &g_buzzer_device;
}
```

### 第二步：注册设备

在 `main.c` 中添加注册代码：

```c
#include "drivers/buzzer.h"

void app_main(void)
{
    // 注册设备
    device_register(Buzzer_get_device());

    // 初始化所有已注册设备
    device_init_all();

    // 使用设备
    struct Device *buzzer = device_find("buzzer");
    if (buzzer) {
        uint8_t on = 1;
        buzzer->ops->write(buzzer->data, &on, 1);  // 蜂鸣器响
    }
}
```

### 第三步：更新构建脚本

在 `main/CMakeLists.txt` 的 `SRCS` 列表中添加：

```cmake
idf_component_register(
    SRCS
        "main.c"
        "framework/framework.c"
        "framework/array.c"
        "drivers/buzzer.c"       # ← 新增
        # ... 其他源文件
    INCLUDE_DIRS "."
)
```


---

## 📖 API 说明

### 框架层

| 函数 | 描述 |
|------|------|
| `device_register(dev)` | 注册设备到全局链表 |
| `device_find(name)` | 按名称查找设备，返回设备指针 |
| `device_init_all()` | 依次初始化所有已注册设备 |
| `device_for_each(callback, arg)` | 遍历所有设备，对每个设备调用回调 |
| `device_count()` | 返回已注册设备数量 |

### 设备操作接口

所有设备必须实现 `DeviceOps` 结构体：

```c
struct DeviceOps {
    int (*init)  (void *self);
    int (*read)  (void *self, void *buf, size_t len);
    int (*write) (void *self, const void *buf, size_t len);
    int (*deinit)(void *self);
};
```

| 函数 | 描述 | 返回值 |
|------|------|--------|
| `init` | 初始化硬件 | 0 = 成功，负数 = 错误码 |
| `read` | 从设备读取数据到 buf | 实际读取的字节数，负数 = 错误 |
| `write` | 向设备写入 buf 中的数据 | 实际写入的字节数，负数 = 错误 |
| `deinit` | 释放硬件资源 | 0 = 成功 |

### Device 结构体

```c
struct Device {
    const char *name;           // 设备名称（唯一标识）
    void *data;                 // 设备私有数据指针
    const struct DeviceOps *ops;// 设备操作函数表
};
```

---

## 🔧 内置驱动速查

| 驱动 | 名称 | 硬件 | 说明 |
|------|------|------|------|
| display | `"display"` | ST7789 240x240 LCD | SPI 接口，RGB565 |
| backlight | `"backlight"` | LEDC PWM (GPIO16) | 0~100% 调光 |
| battery | `"battery"` | ADC1_CH8 (GPIO18) | 分压检测锂电池电压 |
| key | `"key"` | GPIO46/A, GPIO45/B | 按键轮询 |
| internal_temp | `"internal_temp"` | ESP32 内部温度传感器 | 读取芯片温度 |
| ws2812b | `"ws2812b"` | GPIO41 (RMT) | RGB LED，与 SD 卡冲突 |
| uart_ph2 | `"uart_ph2"` | UART1 (GPIO5/6) | PH2.0 串口 |
| sys_uptime | `"sys_uptime"` | FreeRTOS | 系统运行时间 |
| led | `"led"` | 虚拟设备 | 示例驱动 |
| sensor | `"sensor"` | 虚拟设备 | 示例驱动 |

### ⚠️ GPIO 冲突提醒

**GPIO41** 同时连接了 **WS2812B** 和 **SD 卡 CMD**，两者不能同时使用。
- 使用 WS2812B 时，请勿挂载 SD 卡
- 使用 SD 卡时，请勿注册 WS2812B 驱动

---

## ⚙️ 配置说明

### sdkconfig.defaults 关键配置

| 配置项 | 值 | 说明 |
|--------|-----|------|
| `CONFIG_IDF_TARGET` | `esp32s3` | 目标芯片 |
| `CONFIG_ESPTOOLPY_FLASHSIZE_16MB` | `y` | 16MB Flash |
| `CONFIG_ESPTOOLPY_FLASHMODE_QIO` | `y` | QIO 模式 |
| `CONFIG_SPIRAM` | `y` | 启用 PSRAM |
| `CONFIG_SPIRAM_MODE_OCT` | `y` | 八线 PSRAM (8MB) |
| `CONFIG_SPIRAM_USE_MALLOC` | `y` | malloc 可分配 PSRAM |
| `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240` | `y` | CPU 240MHz |

---

## 📊 硬件资源映射

| 类别 | 引脚 | 功能 | 驱动 |
|------|------|------|------|
| LCD | GPIO21/17/15/14 | SPI MOSI/SCLK/CS/DC | display |
| 背光 | GPIO16 | LEDC PWM | backlight |
| 电池 | GPIO18 | ADC1_CH8 | battery |
| 按键 A | GPIO46 | 输入（低有效） | key |
| 按键 B | GPIO45 | 输入（低有效） | key |
| BOOT | GPIO0 | 输入（低有效） | — |
| WS2812 | GPIO41 | RMT 输出 | ws2812b |
| SD 卡 | GPIO38-42 | SDMMC | — |
| UART PH2 | GPIO5/6 | UART1 TX/RX | uart_ph2 |
| USB | GPIO19/20 | USB D-/D+ | — |

---


## 📄 许可证

MIT License — 详见 [LICENSE](LICENSE) 文件。

---

## 📚 相关资源

- [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/)
- [ST7789 数据手册](https://www.rhydolabz.com/documents/33/ST7789.pdf)
- [ES8311 音频编解码器](https://www.everest-semi.com/)
- [ESP32-S3 技术参考手册](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_en.pdf)
