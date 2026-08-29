/**
 * @file framework.c
 * @brief 设备框架实现
 *
 * 实现设备链表管理、注册、查找、初始化以及线程安全保护。
 */
#include "framework.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "framework";

/** 全局设备链表头（循环双向链表） */
static struct list_head g_device_list = {
    .next = &g_device_list,
    .prev = &g_device_list,
};

/** 保护设备链表的互斥锁（懒加载） */
static SemaphoreHandle_t g_device_mutex = NULL;

/* ------------------------------------------------------------------ */
/* 内部工具函数                                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief 确保互斥锁已创建（懒加载）
 * @return 0 成功，-1 失败
 */
static int ensure_mutex(void)
{
    if (g_device_mutex != NULL)
        return 0;

    g_device_mutex = xSemaphoreCreateMutex();
    if (g_device_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create device mutex");
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

    if (ensure_mutex() != 0)
        return -1;

    xSemaphoreTake(g_device_mutex, portMAX_DELAY);

    /* 检查设备名是否已存在 */
    struct list_head *pos;
    list_for_each(pos, &g_device_list) {
        struct Device *d = container_of(pos, struct Device, node);
        if (strcmp(d->name, dev->name) == 0) {
            ESP_LOGE(TAG, "Device name '%s' already exists", dev->name);
            xSemaphoreGive(g_device_mutex);
            return -1;
        }
    }

    list_append(&g_device_list, &dev->node);
    xSemaphoreGive(g_device_mutex);

    ESP_LOGD(TAG, "Device '%s' registered", dev->name);
    return 0;
}

struct Device *device_find(const char *name)
{
    if (name == NULL || g_device_mutex == NULL)
        return NULL;

    xSemaphoreTake(g_device_mutex, portMAX_DELAY);

    struct list_head *pos;
    list_for_each(pos, &g_device_list) {
        struct Device *dev = container_of(pos, struct Device, node);
        if (strcmp(dev->name, name) == 0) {
            xSemaphoreGive(g_device_mutex);
            return dev;
        }
    }

    xSemaphoreGive(g_device_mutex);
    return NULL;
}

int device_init_all(void)
{
    if (ensure_mutex() != 0)
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
                failed++;
            }
        }
    }

    xSemaphoreGive(g_device_mutex);

    if (failed > 0) {
        ESP_LOGW(TAG, "%d device(s) failed to initialize", failed);
    }

    return 0; /* 始终返回 0，部分设备失败不阻塞整体启动 */
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
