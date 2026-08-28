#include "framework.h"
#include <string.h>

// 全局链表头，静态初始化，指向自己
static struct list_head g_device_list = {
    .next = &g_device_list,
    .prev = &g_device_list,
};

int device_register(struct Device *dev)
{
    if (dev == 0)
    {
        return -1;
    }

    // 把新设备插入到链表尾部
    list_append(&g_device_list, &dev->node);

    return 0;
}

struct Device *device_find(const char *name)
{
    struct list_head *pos;

    // 遍历链表
    for (pos = g_device_list.next; pos != &g_device_list; pos = pos->next)
    {
        struct Device *dev = container_of(pos, struct Device, node);

        if (strcmp(dev->name, name) == 0)
        {
            return dev;
        }
    }

    return 0;
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

    // 可选：把 item 自己重置为独立节点，防止之后误用
    item->next = item;
    item->prev = item;
}