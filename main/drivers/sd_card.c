/**
 * @file sd_card.c
 * @brief SD 卡驱动实现（SDMMC 1-bit 模式）
 */
#include "sd_card.h"
#include "../framework/framework.h"

#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "sdmmc_cmd.h"
#include "ff.h"

#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

static const char *TAG = "sd_card";

static struct SdCard g_sd_card;

/* ------------------------------------------------------------------ */
/* 内部辅助函数                                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief 拼接完整路径（自动处理挂载点前缀）
 */
static void build_full_path(char *out, size_t out_size, const char *path)
{
    if (path[0] == '/')
        snprintf(out, out_size, "%s%s", SD_MOUNT_POINT, path);
    else
        snprintf(out, out_size, "%s/%s", SD_MOUNT_POINT, path);
}

/* ------------------------------------------------------------------ */
/* 设备操作函数                                                       */
/* ------------------------------------------------------------------ */

static int sd_card_init(void *self)
{
    struct SdCard *sd = (struct SdCard *)self;

    ESP_LOGI(TAG, "====== SD 卡初始化 (SDMMC 1-bit) ======");
    ESP_LOGI(TAG, "引脚: CLK=%d, CMD=%d, DAT0=%d",
             SD_PIN_CLK, SD_PIN_CMD, SD_PIN_D0);

    /* 1. 配置 SDMMC 主机 */
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_PROBING;

    /* 2. 配置插槽引脚 */
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk = SD_PIN_CLK;
    slot_config.cmd = SD_PIN_CMD;
    slot_config.d0 = SD_PIN_D0;
    slot_config.d1 = SD_PIN_D1;
    slot_config.d2 = SD_PIN_D2;
    slot_config.d3 = SD_PIN_D3;
    slot_config.width = SD_BUS_WIDTH;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    /* 3. 挂载配置 */
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    /* 4. 尝试挂载 */
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(
        SD_MOUNT_POINT, &host, &slot_config, &mount_config, &sd->card);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SD 卡挂载失败: %s (0x%x)", esp_err_to_name(ret), ret);
        if (ret == ESP_ERR_TIMEOUT)
            ESP_LOGE(TAG, "  超时: 请检查 SD 卡是否插入，或引脚连接是否正确");
        else if (ret == ESP_ERR_INVALID_RESPONSE)
            ESP_LOGE(TAG, "  无效响应: CMD 线可能有问题，或卡未进入 SD 模式");
        return -1;
    }

    /* 5. 读取卡信息 */
    sd->mounted = true;
    sdmmc_card_print_info(stdout, sd->card);

    sd->total_bytes = (uint64_t)sd->card->csd.sector_size * sd->card->csd.capacity;
    strncpy(sd->name, sd->card->cid.name, sizeof(sd->name) - 1);
    sd->name[sizeof(sd->name) - 1] = '\0';

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "SD 卡挂载成功！(SDMMC 1-bit 模式)");
    ESP_LOGI(TAG, "总容量: %llu MB", sd->total_bytes / (1024 * 1024));
    ESP_LOGI(TAG, "卡名称: %s", sd->name);
    ESP_LOGI(TAG, "挂载路径: %s", SD_MOUNT_POINT);
    ESP_LOGI(TAG, "========================================");

    /* 6. 读写验证 */
    const char *test_file = SD_MOUNT_POINT "/_sd_test_ok.txt";
    FILE *f = fopen(test_file, "w");
    if (f)
    {
        fprintf(f, "SD Card SDMMC Test OK\n");
        fclose(f);
        unlink(test_file);
        ESP_LOGI(TAG, "读写验证通过");
    }

    return 0;
}

static int sd_card_read(void *self, void *buf, size_t len)
{
    struct SdCard *sd = (struct SdCard *)self;

    if (!sd->mounted)
        return -1;
    if (len < sizeof(struct SdCard))
        return -1;

    /* 刷新剩余空间 */
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    f_getfree("0:", &fre_clust, &fs);

    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;

    sd->free_bytes = (uint64_t)fre_sect * fs->ssize;
    sd->total_bytes = (uint64_t)tot_sect * fs->ssize;

    memcpy(buf, sd, sizeof(struct SdCard));
    return sizeof(struct SdCard);
}

static int sd_card_write(void *self, const void *buf, size_t len)
{
    (void)self;
    (void)buf;
    (void)len;
    return -1; /* 不支持直接写入，使用文件操作函数 */
}

static int sd_card_deinit(void *self)
{
    struct SdCard *sd = (struct SdCard *)self;

    if (sd->mounted)
    {
        esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, sd->card);
        sd->mounted = false;
        sd->card = NULL;
    }
    return 0;
}

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

/* ------------------------------------------------------------------ */
/* 文件操作辅助函数                                                   */
/* ------------------------------------------------------------------ */

int sd_card_read_file(const char *path, char *buf, size_t max_len)
{
    if (!g_sd_card.mounted)
        return -1;

    char full_path[128];
    build_full_path(full_path, sizeof(full_path), path);

    FILE *f = fopen(full_path, "r");
    if (!f)
        return -1;

    size_t n = fread(buf, 1, max_len - 1, f);
    buf[n] = '\0';
    fclose(f);
    return (int)n;
}

int sd_card_write_file(const char *path, const char *buf, size_t len)
{
    if (!g_sd_card.mounted)
        return -1;

    char full_path[128];
    build_full_path(full_path, sizeof(full_path), path);

    FILE *f = fopen(full_path, "w");
    if (!f)
        return -1;

    size_t n = fwrite(buf, 1, len, f);
    fclose(f);
    return (int)n;
}

int sd_card_append_file(const char *path, const char *buf, size_t len)
{
    if (!g_sd_card.mounted)
        return -1;

    char full_path[128];
    build_full_path(full_path, sizeof(full_path), path);

    FILE *f = fopen(full_path, "a");
    if (!f)
        return -1;

    size_t n = fwrite(buf, 1, len, f);
    fclose(f);
    return (int)n;
}

int sd_card_list_files(const char *dir_path)
{
    if (!g_sd_card.mounted)
        return -1;

    char full_path[128];
    build_full_path(full_path, sizeof(full_path), dir_path);

    DIR *d = opendir(full_path);
    if (!d)
        return -1;

    ESP_LOGI(TAG, "目录 %s:", dir_path);
    struct dirent *ent;
    int count = 0;
    while ((ent = readdir(d)) != NULL)
    {
        ESP_LOGI(TAG, "  %s", ent->d_name);
        count++;
    }
    closedir(d);
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
    return stat(full_path, &st) == 0 ? 1 : 0;
}
