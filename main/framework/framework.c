/**
 * @file framework.c
 * @brief 设备框架实现
 *
 * 实现设备管理：
 *   - 双向链表：用于遍历（device_for_each / device_init_all）
 *   - 哈希表：用于加速按名称查找（device_find）
 *   - 互斥锁：保护并发访问
 */
#include "framework.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "hashmap.h"

static const char *TAG = "framework";

/** 全局设备链表头（循环双向链表）— 用于遍历 */
static struct list_head g_device_list = {
    .next = &g_device_list,
    .prev = &g_device_list,
};

/** 全局设备哈希表 — 用于加速 device_find */
static struct hashmap *g_device_map = NULL;

/** 保护设备链表/哈希表的互斥锁（懒加载） */
static SemaphoreHandle_t g_device_mutex = NULL;

/* ------------------------------------------------------------------ */
/* 内部工具函数                                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief 确保互斥锁和哈希表已创建（懒加载）
 * @return 0 成功，-1 失败
 */
static int ensure_ready(void)
{
    if (g_device_mutex != NULL)
        return 0;

    g_device_mutex = xSemaphoreCreateMutex();
    if (g_device_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create device mutex");
        return -1;
    }

    g_device_map = hashmap_create(0);  /* 0 = 默认桶数 */
    if (g_device_map == NULL) {
        ESP_LOGE(TAG, "Failed to create device hashmap");
        vSemaphoreDelete(g_device_mutex);
        g_device_mutex = NULL;
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* 公开 API                                                           */
/* ------------------------------------------------------------------ */

int device_register(struct Device *dev)
{
    if (dev == NULL || dev->name == NULL || dev->ops == NULL) {
        ESP_LOGE(TAG, "Invalid device parameters");
        return -1;
    }

    if (ensure_ready() != 0)
        return -1;

    xSemaphoreTake(g_device_mutex, portMAX_DELAY);

    /* 用哈希表检查设备名是否已存在（O(1)） */
    if (hashmap_contains(g_device_map, dev->name)) {
        ESP_LOGE(TAG, "Device name '%s' already exists", dev->name);
        xSemaphoreGive(g_device_mutex);
        return -1;
    }

    /* 加入链表（用于遍历） */
    list_append(&g_device_list, &dev->node);

    /* 加入哈希表（用于查找）
       value 就是 dev 指针本身，key 是设备名（hashmap 内部会拷贝） */
    hashmap_put(g_device_map, dev->name, dev);

    /* 初始状态：未初始化 */
    dev->state = DEV_UNINIT;

    xSemaphoreGive(g_device_mutex);

    ESP_LOGD(TAG, "Device '%s' registered", dev->name);
    return 0;
}

struct Device *device_find(const char *name)
{
    if (name == NULL || g_device_map == NULL)
        return NULL;

    xSemaphoreTake(g_device_mutex, portMAX_DELAY);

    /* 哈希表查找（O(1) 平均） */
    struct Device *dev = (struct Device *)hashmap_get(g_device_map, name);

    xSemaphoreGive(g_device_mutex);
    return dev;
}

int device_init_all(void)
{
    if (ensure_ready() != 0)
        return -1;

    xSemaphoreTake(g_device_mutex, portMAX_DELAY);

    int failed = 0;
    struct list_head *pos;
    list_for_each(pos, &g_device_list) {
        struct Device *dev = container_of(pos, struct Device, node);
        if (dev->ops && dev->ops->init) {
            int ret = dev->ops->init(dev->data);
            if (ret != 0) {
                ESP_LOGE(TAG, "Device '%s' init failed (ret=%d)", dev->name, ret);
                dev->state = DEV_ERROR;
                failed++;
            } else {
                dev->state = DEV_READY;
            }
        }
    }

    xSemaphoreGive(g_device_mutex);

    if (failed > 0) {
        ESP_LOGW(TAG, "%d device(s) failed to initialize", failed);
    }

    return failed;
}

int device_deinit_all(void)
{
    if (g_device_mutex == NULL)
        return -1;

    xSemaphoreTake(g_device_mutex, portMAX_DELAY);

    int failed = 0;
    struct list_head *pos;
    list_for_each(pos, &g_device_list) {
        struct Device *dev = container_of(pos, struct Device, node);
        /* 只反初始化已就绪的设备 */
        if (dev->state == DEV_READY && dev->ops && dev->ops->deinit) {
            int ret = dev->ops->deinit(dev->data);
            if (ret != 0) {
                ESP_LOGE(TAG, "Device '%s' deinit failed (ret=%d)", dev->name, ret);
                failed++;
            } else {
                dev->state = DEV_UNINIT;
            }
        }
    }

    xSemaphoreGive(g_device_mutex);

    if (failed > 0)
        ESP_LOGW(TAG, "%d device(s) failed to deinitialize", failed);

    return failed;
}

int device_unregister(struct Device *dev)
{
    if (dev == NULL || dev->name == NULL)
        return -1;

    if (g_device_mutex == NULL)
        return -1;

    xSemaphoreTake(g_device_mutex, portMAX_DELAY);

    /* 从哈希表移除 */
    void *removed = hashmap_remove(g_device_map, dev->name);
    if (removed == NULL) {
        ESP_LOGW(TAG, "Device '%s' not found for unregister", dev->name);
        xSemaphoreGive(g_device_mutex);
        return -1;
    }

    /* 从链表移除 */
    list_del(&dev->node);

    /* 重置状态 */
    dev->state = DEV_UNINIT;

    xSemaphoreGive(g_device_mutex);

    ESP_LOGD(TAG, "Device '%s' unregistered", dev->name);
    return 0;
}

void device_for_each(void (*callback)(struct Device *dev, void *arg), void *arg)
{
    if (callback == NULL || g_device_mutex == NULL)
        return;

    xSemaphoreTake(g_device_mutex, portMAX_DELAY);

    struct list_head *pos;
    list_for_each(pos, &g_device_list) {
        struct Device *dev = container_of(pos, struct Device, node);
        callback(dev, arg);
    }

    xSemaphoreGive(g_device_mutex);
}

/* ------------------------------------------------------------------ */
/* 链表操作                                                           */
/* ------------------------------------------------------------------ */

void list_append(struct list_head *head, struct list_head *new_node)
{
    new_node->next = head;
    new_node->prev = head->prev;
    head->prev->next = new_node;
    head->prev = new_node;
}

void list_del(struct list_head *item)
{
    item->prev->next = item->next;
    item->next->prev = item->prev;
    item->next = item;
    item->prev = item;
}
