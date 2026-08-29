/**
 * @file array.c
 * @brief 动态数组实现
 */
#include "array.h"

#include <stdlib.h>
#include <string.h>

void array_init(struct Array *arr, size_t element_size)
{
    arr->data         = NULL;
    arr->size         = 0;
    arr->capacity     = 0;
    arr->element_size = element_size;
}

int array_push_back(struct Array *arr, const void *element)
{
    /* 容量不足时扩容（2 倍增长） */
    if (arr->size >= arr->capacity) {
        size_t new_capacity = (arr->capacity == 0) ? 4 : arr->capacity * 2;
        void *new_data = realloc(arr->data, new_capacity * arr->element_size);
        if (!new_data)
            return -1;

        arr->data     = new_data;
        arr->capacity = new_capacity;
    }

    memcpy((char *)arr->data + arr->size * arr->element_size,
           element, arr->element_size);
    arr->size++;
    return 0;
}

int array_get(const struct Array *arr, size_t index, void *out)
{
    if (index >= arr->size)
        return -1;

    memcpy(out,
           (const char *)arr->data + index * arr->element_size,
           arr->element_size);
    return 0;
}

int array_remove(struct Array *arr, size_t index)
{
    if (index >= arr->size)
        return -1;

    size_t bytes_to_move = (arr->size - index - 1) * arr->element_size;

    if (bytes_to_move > 0) {
        char *dest = (char *)arr->data + index * arr->element_size;
        char *src  = dest + arr->element_size;
        memmove(dest, src, bytes_to_move);
    }

    arr->size--;
    return 0;
}

size_t array_size(const struct Array *arr)
{
    return arr->size;
}

void array_clear(struct Array *arr)
{
    arr->size = 0;
}

void array_free(struct Array *arr)
{
    free(arr->data);
    arr->data     = NULL;
    arr->size     = 0;
    arr->capacity = 0;
}
