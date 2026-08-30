/**
 * @file framework.h
 * @brief 设备框架核心头文件
 *
 * 提供统一的设备模型、双向链表管理、设备注册/查找/初始化接口。
 * 所有设备驱动与应用层均基于此框架开发。
 */
#ifndef FRAMEWORK_H
#define FRAMEWORK_H

#include <stddef.h>

/**
 * @brief 通用双向链表节点
 *
 * 嵌入到需要链表管理的数据结构中，类似于 Linux 内核链表。
 */
struct list_head {
    struct list_head *next; ///< 指向下一个节点
    struct list_head *prev; ///< 指向前一个节点
};

/**
 * @brief 通过成员指针获取包含该成员的结构体指针
 * @param ptr    成员指针
 * @param type   包含该成员的结构体类型
 * @param member 成员在结构体中的名称
 */
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/**
 * @brief 遍历链表宏
 * @param pos  当前遍历到的节点指针
 * @param head 链表头
 */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * @brief 设备操作函数表
 *
 * 每个设备驱动必须实现此结构体中的函数指针，
 * 以便框架进行统一管理和调用。
 *
 * 返回值约定：
 *   - init:   0 成功，-1 失败
 *   - read:   >0 实际读取字节数，-1 失败
 *   - write:  >0 实际写入字节数，-1 失败
 *   - deinit: 0 成功，-1 失败
 */
struct DeviceOps {
    int  (*init)(void *self);                               ///< 初始化设备
    int  (*read)(void *self, void *buf, size_t len);        ///< 读设备数据
    int  (*write)(void *self, const void *buf, size_t len); ///< 写设备数据
    int  (*deinit)(void *self);                             ///< 反初始化设备
};

/**
 * @brief 设备运行状态
 *
 * 由框架在 init/deinit 时自动维护，上层可通过此字段
 * 判断设备是否可用，避免对未初始化或出错的设备操作。
 */
enum DeviceState {
    DEV_UNINIT = 0,  ///< 未初始化（注册后、init 前）
    DEV_READY,       ///< 已初始化、可用
    DEV_ERROR,       ///< 初始化失败或已出错
};

/**
 * @brief 设备结构体
 *
 * 描述一个具体设备，包含名称、私有数据、操作表、状态和链表节点。
 * 所有设备通过 device_register() 注册到全局设备链表中。
 */
struct Device {
    const char *name;              ///< 设备名称（唯一标识）
    void *data;                    ///< 指向设备私有数据
    const struct DeviceOps *ops;   ///< 设备操作表指针
    enum DeviceState state;         ///< 设备运行状态（框架维护）
    struct list_head node;         ///< 链表节点（用于设备链表）
};

/**
 * @brief 注册设备到全局设备链表
 * @param dev  指向待注册设备的指针
 * @return 0 成功；-1 失败（参数无效或设备名重复）
 */
int device_register(struct Device *dev);

/**
 * @brief 按名称查找设备
 * @param name 设备名称
 * @return 找到的设备指针；未找到返回 NULL
 */
struct Device *device_find(const char *name);

/**
 * @brief 初始化所有已注册设备
 *
 * 遍历设备链表，对每个设备调用其 ops->init。
 * 若某设备初始化失败，记录错误并设置状态为 DEV_ERROR，但继续初始化其他设备。
 *
 * @return 0 全部成功；>0 失败设备数；-1 框架错误（互斥锁未创建）
 */
int device_init_all(void);

/**
 * @brief 反初始化所有已注册设备
 *
 * 遍历设备链表，对每个处于 DEV_READY 状态的设备调用其 ops->deinit。
 * 用于系统休眠、OTA 升级前优雅关闭外设。
 *
 * @return 0 全部成功；>0 失败设备数；-1 框架错误
 */
int device_deinit_all(void);

/**
 * @brief 注销设备
 *
 * 从全局设备链表和哈希表中移除指定设备。
 * 调用者负责释放设备结构体本身及其私有数据的内存。
 *
 * @param dev 指向待注销设备的指针
 * @return 0 成功；-1 失败（参数无效或设备不存在）
 */
int device_unregister(struct Device *dev);

/**
 * @brief 遍历所有已注册设备
 * @param callback 回调函数，对每个设备调用一次
 * @param arg      传递给回调函数的用户参数
 *
 * 回调函数原型：void callback(struct Device *dev, void *arg);
 */
void device_for_each(void (*callback)(struct Device *dev, void *arg), void *arg);

/* ===== 链表基础操作 ===== */

/** @brief 在链表头部追加节点 */
void list_append(struct list_head *head, struct list_head *new_node);

/** @brief 从链表中删除节点 */
void list_del(struct list_head *item);

#endif /* FRAMEWORK_H */
