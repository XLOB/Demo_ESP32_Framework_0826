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

/** 全局设备链表头 */
static struct list_head g_device_list = {
    .next = &g_device_list,
    .prev = &g_device_list,
};

/** 保护设备链表的互斥锁 */
static SemaphoreHandle_t g_device_mutex = NULL;

int device_register(struct Device *dev)
{
    if (dev == NULL || dev->name == NULL || dev->ops == NULL)
    {
        ESP_LOGE(TAG, "Invalid device parameters");
        return -1;
    }

    if (g_device_mutex == NULL)
    {
        g_device_mutex = xSemaphoreCreateMutex();
        if (g_device_mutex == NULL)
        {
            ESP_LOGE(TAG, "Failed to create mutex");
            return -1;
        }
    }

    xSemaphoreTake(g_device_mutex, portMAX_DELAY);

    /* 检查重名 */
    struct list_head *pos;
    list_for_each(pos, &g_device_list)
    {
        struct Device *d = container_of(pos, struct Device, node);
        if (strcmp(d->name, dev->name) == 0)
        {
            ESP_LOGE(TAG, "Device name '%s' already exists", dev->name);
            xSemaphoreGive(g_device_mutex);
            return -1;
        }
    }

    list_append(&g_device_list, &dev->node);
    xSemaphoreGive(g_device_mutex);
    return 0;
}

struct Device *device_find(const char *name)
{
    if (name == NULL || g_device_mutex == NULL)
    {
        return NULL;
    }

    xSemaphoreTake(g_device_mutex, portMAX_DELAY);
    struct list_head *pos;
    list_for_each(pos, &g_device_list)
    {
        struct Device *dev = container_of(pos, struct Device, node);
        if (strcmp(dev->name, name) == 0)
        {
            xSemaphoreGive(g_device_mutex);
            return dev;
        }
    }
    xSemaphoreGive(g_device_mutex);
    return NULL;
}

int device_init_all(void)
{
    if (g_device_mutex == NULL)
    {
        return -1;
    }

    xSemaphoreTake(g_device_mutex, portMAX_DELAY);
    struct list_head *pos;
    list_for_each(pos, &g_device_list)
    {
        struct Device *dev = container_of(pos, struct Device, node);
        if (dev->ops && dev->ops->init)
        {
            int ret = dev->ops->init(dev->data);
            if (ret != 0)
            {
                ESP_LOGE(TAG, "Device '%s' init failed (ret=%d)", dev->name, ret);
            }
        }
    }
    xSemaphoreGive(g_device_mutex);
    return 0;
}

void device_for_each(void (*callback)(struct Device *dev, void *arg), void *arg)
{
    if (callback == NULL || g_device_mutex == NULL)
    {
        return;
    }

    xSemaphoreTake(g_device_mutex, portMAX_DELAY);
    struct list_head *pos;
    list_for_each(pos, &g_device_list)
    {
        struct Device *dev = container_of(pos, struct Device, node);
        callback(dev, arg);
    }
    xSemaphoreGive(g_device_mutex);
}

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