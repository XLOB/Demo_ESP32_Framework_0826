/**
 * @file hashmap.h
 * @brief 字符串哈希表
 *
 * 以 NUL 结尾的字符串为 key，void* 为 value 的哈希表。
 * 采用链地址法（separate chaining）解决哈希冲突。
 *
 * TODO: 核心算法由你实现
 */
#ifndef HASHMAP_H
#define HASHMAP_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 哈希表默认桶数量 */
#define HASHMAP_DEFAULT_BUCKETS  32

/** 哈希表条目（链表节点） */
struct hashmap_entry {
    char *key;                  ///< 键（字符串，已拷贝）
    void *value;                ///< 值指针
    struct hashmap_entry *next; ///< 下一个条目（冲突链）
};

/** 哈希表 */
struct hashmap {
    struct hashmap_entry **buckets;  ///< 桶数组
    size_t bucket_count;             ///< 桶数量
    size_t size;                     ///< 元素总数
};

/* ------------------------------------------------------------------ */
/* 生命周期                                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief 创建哈希表
 *
 * @param bucket_count  桶数量（建议 2 的幂，0 表示使用默认值）
 * @return 哈希表指针，失败返回 NULL
 */
struct hashmap *hashmap_create(size_t bucket_count);

/**
 * @brief 销毁哈希表
 *
 * 释放所有桶、条目、以及拷贝的 key 字符串。
 * 注意：不释放 value 指向的内存（调用者负责）。
 *
 * @param map  哈希表指针
 */
void hashmap_destroy(struct hashmap *map);

/* ------------------------------------------------------------------ */
/* 基础操作                                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief 插入键值对
 *
 * 如果 key 已存在，更新 value 并返回旧 value（调用者需自行释放）。
 * 如果 key 不存在，插入新条目，返回 NULL。
 *
 * @param map    哈希表指针
 * @param key    键（字符串，内部会拷贝一份）
 * @param value  值指针
 * @return 旧值指针（如果是替换）；NULL（如果是新插入）
 */
void *hashmap_put(struct hashmap *map, const char *key, void *value);

/**
 * @brief 查找值
 *
 * @param map  哈希表指针
 * @param key  键
 * @return 值指针；未找到返回 NULL
 */
void *hashmap_get(struct hashmap *map, const char *key);

/**
 * @brief 删除条目
 *
 * @param map  哈希表指针
 * @param key  键
 * @return 被删除条目的 value 指针（调用者需自行释放）；未找到返回 NULL
 */
void *hashmap_remove(struct hashmap *map, const char *key);

/**
 * @brief 检查 key 是否存在
 *
 * @param map  哈希表指针
 * @param key  键
 * @return true 存在；false 不存在
 */
bool hashmap_contains(struct hashmap *map, const char *key);

/**
 * @brief 获取元素数量
 *
 * @param map  哈希表指针
 * @return 元素个数
 */
size_t hashmap_size(struct hashmap *map);

/* ------------------------------------------------------------------ */
/* 遍历                                                               */
/* ------------------------------------------------------------------ */

/**
 * @brief 遍历回调函数类型
 *
 * @param key    键
 * @param value  值
 * @param arg    用户参数
 * @return 返回 false 可终止遍历
 */
typedef bool (*hashmap_iter_cb)(const char *key, void *value, void *arg);

/**
 * @brief 遍历所有条目
 *
 * 对每个条目调用一次回调函数。
 * 如果回调返回 false，立即停止遍历。
 *
 * @param map       哈希表指针
 * @param callback  回调函数
 * @param arg       用户参数
 */
void hashmap_foreach(struct hashmap *map, hashmap_iter_cb callback, void *arg);

#ifdef __cplusplus
}
#endif

#endif /* HASHMAP_H */
