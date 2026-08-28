#ifndef FRAMEWORK_H
#define FRAMEWORK_H

#include <stddef.h>

// 通用链表节点
struct list_head
{
    struct list_head *next;
    struct list_head *prev;
};

// 从成员地址反推结构体地址
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// 设备操作表
struct DeviceOps
{
    int (*init)(void *self);
    int (*read)(void *self, void *buf, size_t len);
    int (*write)(void *self, const void *buf, size_t len);
    int (*deinit)(void *self);
};

// 设备结构体
struct Device
{
    const char *name;
    void *data;
    const struct DeviceOps *ops;

    struct list_head node; // 让设备变成链表节点
};

// 框架接口
int device_register(struct Device *dev);
struct Device *device_find(const char *name);

// 常用函数
void list_append(struct list_head *head, struct list_head *new_node);
void list_del(struct list_head *item);
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#endif