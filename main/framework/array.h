#ifndef ARRAY_H
#define ARRAY_H

#include <stddef.h>

struct Array
{
    void *data;
    size_t size;
    size_t capacity;
    size_t element_size;
};

void array_init(struct Array *arr, size_t element_size);
int array_push_back(struct Array *arr, const void *element);
int array_get(const struct Array *arr, size_t index, void *out);
size_t array_size(const struct Array *arr);
void array_free(struct Array *arr);
int array_remove(struct Array *arr, size_t index);
void array_clear(struct Array *arr);

#endif // ARRAY_H