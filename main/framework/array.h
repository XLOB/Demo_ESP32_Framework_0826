/**
 * @file array.h
 * @brief 动态数组（泛型）
 *
 * 提供类型无关的动态数组，用于存储任意大小的元素。
 * 自动扩容（2 倍增长），支持 push_back、get、remove 等操作。
 */
#ifndef ARRAY_H
#define ARRAY_H

#include <stddef.h>

/** 动态数组结构体 */
struct Array {
    void   *data;         ///< 数据缓冲区
    size_t  size;         ///< 当前元素个数
    size_t  capacity;     ///< 已分配容量（元素个数）
    size_t  element_size; ///< 单个元素大小（字节）
};

/**
 * @brief 初始化动态数组
 * @param arr          数组结构体指针
 * @param element_size 单个元素的大小（字节）
 */
void array_init(struct Array *arr, size_t element_size);

/**
 * @brief 在末尾追加元素
 * @param arr      数组结构体指针
 * @param element  待追加元素的指针
 * @return 0 成功；-1 失败（内存分配失败）
 */
int array_push_back(struct Array *arr, const void *element);

/**
 * @brief 获取指定索引的元素
 * @param arr    数组结构体指针
 * @param index  元素索引（从 0 开始）
 * @param out    输出缓冲区，用于存储获取到的元素
 * @return 0 成功；-1 失败（索引越界）
 */
int array_get(const struct Array *arr, size_t index, void *out);

/**
 * @brief 删除指定索引的元素（后续元素前移）
 * @param arr    数组结构体指针
 * @param index  要删除的元素索引
 * @return 0 成功；-1 失败（索引越界）
 */
int array_remove(struct Array *arr, size_t index);

/**
 * @brief 获取数组当前元素个数
 * @param arr 数组结构体指针
 * @return 元素个数
 */
size_t array_size(const struct Array *arr);

/**
 * @brief 清空数组（不释放内存，仅 size 置 0）
 * @param arr 数组结构体指针
 */
void array_clear(struct Array *arr);

/**
 * @brief 释放数组内存并重置状态
 * @param arr 数组结构体指针
 */
void array_free(struct Array *arr);

#endif /* ARRAY_H */
