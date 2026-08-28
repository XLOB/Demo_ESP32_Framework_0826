#include <string.h> // 为了 memcpy
#include <stdlib.h> // 为了 realloc / free

#include "array.h"

void array_init(struct Array *arr, size_t element_size)
{
    arr->data = NULL;
    arr->size = 0;
    arr->capacity = 0;
    arr->element_size = element_size;
}

int array_push_back(struct Array *arr, const void *element)
{
    if (arr->size >= arr->capacity)
    {
        size_t new_capacity = (arr->capacity == 0) ? 1 : arr->capacity * 2;
        void *new_data = realloc(arr->data, new_capacity * arr->element_size);
        if (!new_data)
        {
            return -1; // Allocation failed
        }
        arr->data = new_data;
        arr->capacity = new_capacity;
    }

    memcpy((char *)arr->data + arr->size * arr->element_size, element, arr->element_size);
    arr->size++;
    return 0; // Success
}

int array_get(const struct Array *arr, size_t index, void *out)
{
    if (index < arr->size)
    {
        memcpy(out, (char *)arr->data + index * arr->element_size, arr->element_size);
        return 0; // Success
    }
    return -1; // Index out of bounds
}

size_t array_size(const struct Array *arr)
{
    return arr->size;
}

void array_free(struct Array *arr)
{
    free(arr->data);
    arr->data = NULL;
    arr->size = 0;
    arr->capacity = 0;
}