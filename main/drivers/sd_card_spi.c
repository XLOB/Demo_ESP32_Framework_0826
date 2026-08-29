/**
 * @file sd_card_spi.c
 * @brief TF 卡 (MicroSD) 驱动 - SPI 模式
 *
 * 使用 SPI2 外设驱动 SD 卡（SPI 模式兼容性更好，对信号质量要求更低）。
 * 适用于 SDMMC 模式因 WS2812 干扰等原因无法工作的情况。
 *
 * 硬件: AI-VOX3 ESP32-S3
 *   SPI CLK  = GPIO40 (SD CLK)
 *   SPI MOSI = GPIO41 (SD CMD)   ← 与 WS2812 共用
 *   SPI MISO = GPIO42 (SD DAT0)
 *   SPI CS   = GPIO39 (SD DAT3)
 *
 * 挂载路径: /sdcard
 * 文件操作: 标准 C 库 fopen/fread/fwrite 等
 *
 * 性能参考: ~1-2 MB/s (SPI 模式，低于 SDMMC 1-bit 的 ~2-4 MB/s)
 */

#include "sd_card.h"
#include "../framework/framework.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

static const char *TAG = "sd_card";

// ===== SPI 配置 =====
#define SD_SPI_HOST       SPI2_HOST   // 使用 SPI2（SPI3 已被 LCD 占用）
#define SD_SPI_FREQ_KHZ   400         // 极低频率 400kHz（先保证能通信）
#define SD_PIN_CS         39          // CS 片选 = DAT3

// 全局 SD 卡实例
static struct SdCard g_sd_card;

// 挂载重试次数和间隔
#define SD_MOUNT_RETRIES       3
#define SD_MOUNT_RETRY_DELAY_MS  300

// SPI 总线是否已初始化（避免重复初始化）
static bool s_spi_bus_inited = false;

// ===== 设备操作函数 =====

/**
 * @brief 释放 SD 卡相关引脚为高阻输入
 */
static void release_sd_pins(void)
{
    ESP_LOGI(TAG, "释放 SD 卡 SPI 引脚为高阻输入...");

    gpio_reset_pin(SD_PIN_CLK);
    gpio_reset_pin(SD_PIN_CMD);   // MOSI
    gpio_reset_pin(SD_PIN_D0);    // MISO
    gpio_reset_pin(SD_PIN_CS);    // CS

    vTaskDelay(pdMS_TO_TICKS(20));
}

/**
 * @brief 初始化 SPI 总线（只做一次）
 */
static esp_err_t init_spi_bus(void)
{
    if (s_spi_bus_inited)
        return ESP_OK;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_PIN_CMD,   // MOSI = CMD
        .miso_io_num = SD_PIN_D0,    // MISO = DAT0
        .sclk_io_num = SD_PIN_CLK,   // SCLK = CLK
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,     // 单次最大传输 4KB
    };

    esp_err_t ret = spi_bus_initialize(SD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_OK)
    {
        s_spi_bus_inited = true;
        ESP_LOGI(TAG, "SPI 总线初始化成功");
    }
    else if (ret == ESP_ERR_INVALID_STATE)
    {
        // 总线已被其他模块初始化，也视为成功
        s_spi_bus_inited = true;
        ESP_LOGW(TAG, "SPI 总线已存在，复用现有总线");
    }
    else
    {
        ESP_LOGE(TAG, "SPI 总线初始化失败: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief SD 卡 SPI 模式唤醒序列
 *  SD 卡进入 SPI 模式需要：CS 拉高 + 74 个以上空时钟 + CS 拉低 + CMD0
 *  这里手动做一个唤醒序列，确保卡处于就绪状态
 */
static void sd_card_spi_wakeup(void)
{
    // 先把 CS 设为输出高电平（卡未选中状态）
    gpio_set_direction(SD_PIN_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(SD_PIN_CS, 1);

    // 等待卡上电稳定（有些卡需要 250ms+）
    vTaskDelay(pdMS_TO_TICKS(300));

    // 手动发 80 个空时钟（CS 高电平期间）
    // 使用 GPIO 模拟，确保卡检测到 SPI 模式的初始时钟
    gpio_set_direction(SD_PIN_CLK, GPIO_MODE_OUTPUT);
    gpio_set_direction(SD_PIN_CMD, GPIO_MODE_OUTPUT);
    gpio_set_level(SD_PIN_CMD, 1);  // MOSI 保持高

    for (int i = 0; i < 80; i++)
    {
        gpio_set_level(SD_PIN_CLK, 0);
        esp_rom_delay_us(2);
        gpio_set_level(SD_PIN_CLK, 1);
        esp_rom_delay_us(2);
    }

    // 结束后把引脚恢复为高阻，交给 SPI 驱动接管
    gpio_set_direction(SD_PIN_CLK, GPIO_MODE_INPUT);
    gpio_set_direction(SD_PIN_CMD, GPIO_MODE_INPUT);
    gpio_set_direction(SD_PIN_CS, GPIO_MODE_INPUT);

    ESP_LOGI(TAG, "SD 卡 SPI 唤醒序列已发送（80个空时钟）");
}

/**
 * @brief 尝试挂载 SD 卡（SPI 模式）
 */
static esp_err_t try_mount_spi(struct SdCard *sd)
{
    // ===== 1. 确保 SPI 总线已初始化 =====
    esp_err_t ret = init_spi_bus();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
        return ret;

    // ===== 2. 配置 SDSPI 主机 =====
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;
    host.max_freq_khz = SD_SPI_FREQ_KHZ;

    ESP_LOGI(TAG, "SPI 主机: slot=%d, freq=%d kHz",
             host.slot, host.max_freq_khz);
    ESP_LOGI(TAG, "引脚: CLK=%d, MOSI(CMD)=%d, MISO(D0)=%d, CS=%d",
             SD_PIN_CLK, SD_PIN_CMD, SD_PIN_D0, SD_PIN_CS);

    // ===== 3. 配置 SDSPI 设备 =====
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_PIN_CS;
    slot_config.host_id = SD_SPI_HOST;

    // ===== 4. FATFS 挂载配置 =====
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,   // 失败不自动格式化
        .max_files = 5,                     // 最多同时打开 5 个文件
        .allocation_unit_size = 16 * 1024,  // 簇大小 16KB
    };

    // ===== 5. 挂载 =====
    ret = esp_vfs_fat_sdspi_mount(
        SD_MOUNT_POINT,
        &host,
        &slot_config,
        &mount_config,
        &sd->card);

    return ret;
}

/**
 * @brief 初始化 SD 卡（SPI 模式）
 */
static int sd_card_init(void *self)
{
    struct SdCard *sd = (struct SdCard *)self;

    // 0. 先释放引脚
    release_sd_pins();

    // 0.5. 发送 SPI 唤醒序列（CS 高 + 74+ 空时钟）
    sd_card_spi_wakeup();

    // 1. 多次尝试挂载
    esp_err_t ret = ESP_FAIL;
    for (int i = 0; i < SD_MOUNT_RETRIES; i++)
    {
        ESP_LOGI(TAG, "第 %d/%d 次尝试挂载 (SPI 模式)...", i + 1, SD_MOUNT_RETRIES);
        ret = try_mount_spi(sd);
        if (ret == ESP_OK)
            break;

        ESP_LOGW(TAG, "挂载失败: %s (0x%x)", esp_err_to_name(ret), ret);

        if (i < SD_MOUNT_RETRIES - 1)
        {
            ESP_LOGI(TAG, "%dms 后重试...", SD_MOUNT_RETRY_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(SD_MOUNT_RETRY_DELAY_MS));
        }
    }

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "========== SD 卡挂载失败 (SPI 模式) ==========");
        ESP_LOGE(TAG, "错误: %s (0x%x)", esp_err_to_name(ret), ret);
        ESP_LOGE(TAG, "排查步骤:");
        ESP_LOGE(TAG, "  1. 确认 TF 卡已正确插入");
        ESP_LOGE(TAG, "  2. 确认卡格式为 FAT32 + MBR");
        ESP_LOGE(TAG, "  3. 检查 CS 引脚 (GPIO39) 是否连接");
        ESP_LOGE(TAG, "  4. 尝试降低 SD_SPI_FREQ_KHZ 频率");
        ESP_LOGE(TAG, "  5. 检查 WS2812 是否干扰 MOSI 线 (GPIO41)");
        ESP_LOGE(TAG, "============================================");
        sd->mounted = false;
        return -1;
    }

    sd->mounted = true;

    // 2. 读取卡信息
    sdmmc_card_t *card = sd->card;

    sd->total_bytes = ((uint64_t)card->csd.capacity) * card->csd.sector_size;
    memset(sd->name, 0, sizeof(sd->name));
    strncpy(sd->name, (const char *)card->cid.name, sizeof(sd->name) - 1);

    // 3. 打印官方卡信息
    ESP_LOGI(TAG, "========== SD 卡信息 (SPI) ==========");
    sdmmc_card_print_info(stdout, card);

    // 4. 计算剩余空间
    FATFS *fs = NULL;
    DWORD free_clusters = 0;
    if (f_getfree("0:", &free_clusters, &fs) == FR_OK)
    {
        uint64_t free_sectors = (uint64_t)free_clusters * fs->csize;
        sd->free_bytes = free_sectors * card->csd.sector_size;
    }
    else
    {
        sd->free_bytes = 0;
    }

    uint64_t total_mb = sd->total_bytes / (1024 * 1024);
    uint64_t free_mb = sd->free_bytes / (1024 * 1024);

    ESP_LOGI(TAG, "剩余空间: %llu MB / %llu MB (%.1f%%)",
             free_mb, total_mb,
             total_mb > 0 ? (float)free_mb * 100.0f / (float)total_mb : 0);
    ESP_LOGI(TAG, "挂载点: %s", SD_MOUNT_POINT);
    ESP_LOGI(TAG, "====================================");

    // 5. 写入验证
    FILE *test = fopen("/sdcard/_spi_test.tmp", "w");
    if (test)
    {
        fprintf(test, "SPI OK\n");
        fclose(test);
        unlink("/sdcard/_spi_test.tmp");
        ESP_LOGI(TAG, "读写验证: 正常");
    }
    else
    {
        ESP_LOGW(TAG, "警告: 写入测试失败");
    }

    return 0;
}

/**
 * @brief 读取 SD 卡状态信息
 */
static int sd_card_read(void *self, void *buf, size_t len)
{
    struct SdCard *sd = (struct SdCard *)self;

    if (len < sizeof(struct SdCard))
        return -1;

    // 刷新剩余空间
    if (sd->mounted && sd->card)
    {
        FATFS *fs = NULL;
        DWORD free_clusters = 0;
        if (f_getfree("0:", &free_clusters, &fs) == FR_OK)
        {
            uint64_t free_sectors = (uint64_t)free_clusters * fs->csize;
            sd->free_bytes = free_sectors * sd->card->csd.sector_size;
        }
    }

    memcpy(buf, sd, sizeof(struct SdCard));
    return sizeof(struct SdCard);
}

/**
 * @brief 写设备（不支持直接写入）
 */
static int sd_card_write(void *self, const void *buf, size_t len)
{
    return -1;
}

/**
 * @brief 反初始化
 */
static int sd_card_deinit(void *self)
{
    struct SdCard *sd = (struct SdCard *)self;

    if (sd->mounted && sd->card)
    {
        esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, sd->card);
        if (ret == ESP_OK)
            ESP_LOGI(TAG, "SD 卡已安全卸载");
        else
            ESP_LOGE(TAG, "SD 卡卸载失败: %s", esp_err_to_name(ret));
        sd->mounted = false;
        sd->card = NULL;
    }

    // 释放 SPI 总线
    if (s_spi_bus_inited)
    {
        spi_bus_free(SD_SPI_HOST);
        s_spi_bus_inited = false;
    }

    return 0;
}

// ===== 设备注册 =====

static const struct DeviceOps sd_card_ops = {
    .init = sd_card_init,
    .read = sd_card_read,
    .write = sd_card_write,
    .deinit = sd_card_deinit,
};

static struct Device g_sd_card_device = {
    .name = "sd_card",
    .data = &g_sd_card,
    .ops = &sd_card_ops,
};

struct Device *SdCard_get_device(void)
{
    return &g_sd_card_device;
}

// ===== 文件操作辅助函数 =====

static void build_full_path(char *out, size_t out_size, const char *path)
{
    if (path[0] == '/')
        snprintf(out, out_size, "%s%s", SD_MOUNT_POINT, path);
    else
        snprintf(out, out_size, "%s/%s", SD_MOUNT_POINT, path);
}

int sd_card_read_file(const char *path, char *buf, size_t max_len)
{
    if (!g_sd_card.mounted)
    {
        ESP_LOGE(TAG, "SD 卡未挂载");
        return -1;
    }

    char full_path[128];
    build_full_path(full_path, sizeof(full_path), path);

    FILE *f = fopen(full_path, "r");
    if (!f)
    {
        ESP_LOGE(TAG, "打开文件失败: %s", full_path);
        return -1;
    }

    size_t read_len = fread(buf, 1, max_len - 1, f);
    buf[read_len] = '\0';
    fclose(f);

    ESP_LOGD(TAG, "读取文件 %s: %d 字节", full_path, (int)read_len);
    return (int)read_len;
}

int sd_card_write_file(const char *path, const char *buf, size_t len)
{
    if (!g_sd_card.mounted)
    {
        ESP_LOGE(TAG, "SD 卡未挂载");
        return -1;
    }

    char full_path[128];
    build_full_path(full_path, sizeof(full_path), path);

    FILE *f = fopen(full_path, "w");
    if (!f)
    {
        ESP_LOGE(TAG, "创建文件失败: %s", full_path);
        return -1;
    }

    size_t written = fwrite(buf, 1, len, f);
    fclose(f);

    ESP_LOGI(TAG, "写入文件 %s: %d/%d 字节", full_path, (int)written, (int)len);
    return (written == len) ? (int)written : -1;
}

int sd_card_append_file(const char *path, const char *buf, size_t len)
{
    if (!g_sd_card.mounted)
    {
        ESP_LOGE(TAG, "SD 卡未挂载");
        return -1;
    }

    char full_path[128];
    build_full_path(full_path, sizeof(full_path), path);

    FILE *f = fopen(full_path, "a");
    if (!f)
    {
        ESP_LOGE(TAG, "打开文件(追加)失败: %s", full_path);
        return -1;
    }

    size_t written = fwrite(buf, 1, len, f);
    fclose(f);

    ESP_LOGI(TAG, "追加文件 %s: %d 字节", full_path, (int)written);
    return (written == len) ? (int)written : -1;
}

int sd_card_list_files(const char *dir_path)
{
    if (!g_sd_card.mounted)
    {
        ESP_LOGE(TAG, "SD 卡未挂载");
        return -1;
    }

    char full_path[128];
    build_full_path(full_path, sizeof(full_path), dir_path);

    DIR *dir = opendir(full_path);
    if (!dir)
    {
        ESP_LOGE(TAG, "打开目录失败: %s", full_path);
        return -1;
    }

    int count = 0;
    struct dirent *entry;
    ESP_LOGI(TAG, "目录 %s 内容:", full_path);

    while ((entry = readdir(dir)) != NULL)
    {
        char file_path[256];
        snprintf(file_path, sizeof(file_path), "%s/%s", full_path, entry->d_name);

        struct stat st;
        if (stat(file_path, &st) == 0)
        {
            if (S_ISDIR(st.st_mode))
                ESP_LOGI(TAG, "  [DIR]  %s/", entry->d_name);
            else
                ESP_LOGI(TAG, "  [%6d] %s", (int)st.st_size, entry->d_name);
        }
        else
        {
            ESP_LOGI(TAG, "  [???]  %s", entry->d_name);
        }
        count++;
    }

    closedir(dir);
    ESP_LOGI(TAG, "共 %d 个条目", count);
    return count;
}

int sd_card_file_exists(const char *path)
{
    if (!g_sd_card.mounted)
        return -1;

    char full_path[128];
    build_full_path(full_path, sizeof(full_path), path);

    struct stat st;
    if (stat(full_path, &st) == 0)
        return 1;
    return 0;
}
