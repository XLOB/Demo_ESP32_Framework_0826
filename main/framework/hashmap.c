/**
 * @file hashmap.c
 * @brief 字符串哈希表实现
 *
 * 实现方式：链地址法（separate chaining）
 *   - 一个桶数组，每个桶是一个单链表
 *   - key 经过哈希函数映射到桶索引
 *   - 同一个桶里的冲突 key 串在链表上
 *
 * 平均时间复杂度：
 *   put/get/remove : O(1)
 *   foreach        : O(n)
 */
#include "hashmap.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* 内部工具函数                                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief 字符串哈希函数 —— DJB2 算法
 *
 * 经典的 Daniel J. Bernstein 哈希，简单且分布不错。
 * 原理：初始值 5381，每次 hash = hash * 33 + 当前字符。
 * 为什么是 5381 和 33？ 实测分布好、计算快，经验值。
 *
 * 返回 size_t（跟指针一样宽），足够用。
 */
static size_t hash_str(const char *str)
{
    size_t hash = 5381;
    int c;

    while ((c = (unsigned char)*str++) != '\0') {
        /* hash = hash * 33 + c */
        hash = ((hash << 5) + hash) + (size_t)c;
    }

    return hash;
}

/**
 * @brief 计算 key 对应的桶索引
 *
 * 桶数是 2 的幂的话，可以用位运算取模（快）。
 * 不是 2 的幂就用 %（慢一点但也能用）。
 */
static size_t bucket_index(size_t hash, size_t bucket_count)
{
    /* 如果 bucket_count 是 2 的幂，用 & 代替 % */
    if ((bucket_count & (bucket_count - 1)) == 0) {
        return hash & (bucket_count - 1);
    }
    return hash % bucket_count;
}

/**
 * @brief 分配并拷贝一个字符串（模拟 strdup）
 *
 * 不直接用 strdup 是因为有些嵌入式环境不一定有。
 */
static char *my_strdup(const char *s)
{
    size_t len = strlen(s) + 1;
    char *dup = (char *)malloc(len);
    if (dup != NULL) {
        memcpy(dup, s, len);
    }
    return dup;
}

/* ------------------------------------------------------------------ */
/* 生命周期                                                           */
/* ------------------------------------------------------------------ */

struct hashmap *hashmap_create(size_t bucket_count)
{
    /* 参数修正：0 表示用默认值 */
    if (bucket_count == 0) {
        bucket_count = HASHMAP_DEFAULT_BUCKETS;
    }

    /* 分配 hashmap 结构体 */
    struct hashmap *map = (struct hashmap *)malloc(sizeof(struct hashmap));
    if (map == NULL)
        return NULL;

    /* 分配桶数组（bucket_count 个指针） */
    map->buckets = (struct hashmap_entry **)calloc(
        bucket_count, sizeof(struct hashmap_entry *)
    );
    if (map->buckets == NULL) {
        free(map);
        return NULL;
    }

    map->bucket_count = bucket_count;
    map->size = 0;

    return map;
}

void hashmap_destroy(struct hashmap *map)
{
    if (map == NULL)
        return;

    /* 遍历每个桶 */
    for (size_t i = 0; i < map->bucket_count; i++) {
        struct hashmap_entry *entry = map->buckets[i];

        /* 遍历桶里的链表，逐个释放 */
        while (entry != NULL) {
            struct hashmap_entry *next = entry->next;

            free(entry->key);   /* 释放我们 strdup 的 key */
            free(entry);        /* 释放 entry 本身 */

            entry = next;
        }
    }

    /* 释放桶数组和 map */
    free(map->buckets);
    free(map);
}

/* ------------------------------------------------------------------ */
/* 基础操作                                                           */
/* ------------------------------------------------------------------ */

void *hashmap_put(struct hashmap *map, const char *key, void *value)
{
    if (map == NULL || key == NULL)
        return NULL;

    /* 1. 算哈希，找桶 */
    size_t hash = hash_str(key);
    size_t idx = bucket_index(hash, map->bucket_count);

    /* 2. 先看看这个 key 是不是已经存在了 */
    struct hashmap_entry *entry = map->buckets[idx];
    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            /* 找到了！替换 value，返回旧 value */
            void *old_value = entry->value;
            entry->value = value;
            return old_value;
        }
        entry = entry->next;
    }

    /* 3. 没找到，新建一个 entry */
    struct hashmap_entry *new_entry =
        (struct hashmap_entry *)malloc(sizeof(struct hashmap_entry));
    if (new_entry == NULL)
        return NULL;

    new_entry->key = my_strdup(key);   /* 拷贝一份 key，不依赖调用者的内存 */
    if (new_entry->key == NULL) {
        free(new_entry);
        return NULL;
    }
    new_entry->value = value;

    /* 4. 插到链表头部（O(1)，插头部比插尾部简单） */
    new_entry->next = map->buckets[idx];
    map->buckets[idx] = new_entry;

    map->size++;
    return NULL;  /* 新插入，没有旧值 */
}

void *hashmap_get(struct hashmap *map, const char *key)
{
    if (map == NULL || key == NULL)
        return NULL;

    /* 算哈希，找桶 */
    size_t hash = hash_str(key);
    size_t idx = bucket_index(hash, map->bucket_count);

    /* 遍历桶里的链表找 key */
    struct hashmap_entry *entry = map->buckets[idx];
    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }

    return NULL;  /* 没找到 */
}

void *hashmap_remove(struct hashmap *map, const char *key)
{
    if (map == NULL || key == NULL)
        return NULL;

    /* 算哈希，找桶 */
    size_t hash = hash_str(key);
    size_t idx = bucket_index(hash, map->bucket_count);

    struct hashmap_entry *prev = NULL;
    struct hashmap_entry *curr = map->buckets[idx];

    /* 遍历链表找 key，同时记住前一个节点（prev） */
    while (curr != NULL) {
        if (strcmp(curr->key, key) == 0) {
            /* 找到了！从链表中摘下 */
            if (prev == NULL) {
                /* 是桶的第一个节点 */
                map->buckets[idx] = curr->next;
            } else {
                /* 是中间或尾部节点 */
                prev->next = curr->next;
            }

            /* 保存 value，释放 key 和 entry */
            void *value = curr->value;
            free(curr->key);
            free(curr);

            map->size--;
            return value;
        }

        prev = curr;
        curr = curr->next;
    }

    return NULL;  /* 没找到 */
}

bool hashmap_contains(struct hashmap *map, const char *key)
{
    return hashmap_get(map, key) != NULL;
}

size_t hashmap_size(struct hashmap *map)
{
    if (map == NULL)
        return 0;
    return map->size;
}

/* ------------------------------------------------------------------ */
/* 遍历                                                               */
/* ------------------------------------------------------------------ */

void hashmap_foreach(struct hashmap *map, hashmap_iter_cb callback, void *arg)
{
    if (map == NULL || callback == NULL)
        return;

    /* 外层：遍历每个桶 */
    for (size_t i = 0; i < map->bucket_count; i++) {
        struct hashmap_entry *entry = map->buckets[i];

        /* 内层：遍历桶里的链表 */
        while (entry != NULL) {
            /* 先存 next，因为 callback 里可能删除 entry（虽然不推荐） */
            struct hashmap_entry *next = entry->next;

            /* 调用回调，返回 false 就停止 */
            if (!callback(entry->key, entry->value, arg)) {
                return;
            }

            entry = next;
        }
    }
}
