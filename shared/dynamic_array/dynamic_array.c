/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: shared/dynamic_array/dynamic_array.c
 *
 * Purpose:
 *   Implements a heap-backed circular dynamic array for non-hot-path use.
 *
 * Design:
 *   Storage grows by allocating and copying; callers must not use this utility
 *   in interrupt, scheduling, context-switch, or capability hot paths.
 */

#include <stdlib.h>
#include "dynamic_array.h"

DynamicArray *array_init(DynamicArray *array)
{
    if (array == NULL)
        return NULL;

    array->head = 0;
    array->count = 0;
    array->capacity = ARRAY_INITIAL_CAPACITY;
    array->data = (void **)malloc(ARRAY_INITIAL_CAPACITY * sizeof(void *));

    if (array->data == NULL)
    {
        array->capacity = 0;
        return NULL;
    }

    return array;
}

void *array_get(const DynamicArray *array, size_t index)
{
    if (array == NULL || array->data == NULL || index >= array->count)
        return NULL;

    size_t actual_index = array->head + index;
    if (actual_index >= array->capacity)
        actual_index -= array->capacity;

    return array->data[actual_index];
}

bool array_set(DynamicArray *array, size_t index, void *item)
{
    if (array == NULL || array->data == NULL || index >= array->count)
        return false;

    size_t actual_index = array->head + index;
    if (actual_index >= array->capacity)
        actual_index -= array->capacity;

    array->data[actual_index] = item;
    return true;
}

static bool resize(DynamicArray *array)
{
    if (array == NULL || array->capacity == 0)
        return false;

    /* Prevent overflow on multiplication */
    if (array->capacity > (SIZE_MAX / (ARRAY_GROWTH_FACTOR * sizeof(void *))))
        return false;

    const size_t new_capacity = array->capacity * ARRAY_GROWTH_FACTOR;
    void **new_data = (void **)malloc(new_capacity * sizeof(void *));

    if (new_data == NULL)
        return false;

    /* Copy existing elements preserving logical 0..count order */
    const size_t head = array->head;
    const size_t cap = array->capacity;
    const size_t count = array->count;

    for (size_t i = 0; i < count; ++i)
    {
        size_t idx = head + i;
        if (idx >= cap)
            idx -= cap;
        new_data[i] = array->data[idx];
    }

    free(array->data);
    array->data = new_data;
    array->capacity = new_capacity;
    array->head = 0;
    return true;
}

bool array_push(DynamicArray *array, void *item)
{
    if (array == NULL)
        return false;

    if (array->data == NULL && array_init(array) == NULL)
        return false;

    if (array->count >= array->capacity)
    {
        if (!resize(array))
            return false;
    }

    size_t tail = array->head + array->count;
    if (tail >= array->capacity)
        tail -= array->capacity;

    array->data[tail] = item;
    array->count++;
    return true;
}

void *array_pop(DynamicArray *array)
{
    if (array == NULL || array->count == 0 || array->data == NULL)
        return NULL;

    size_t tail = array->head + array->count - 1;
    if (tail >= array->capacity)
        tail -= array->capacity;

    void *item = array->data[tail];
    array->count--;
    return item;
}

bool array_enqueue(DynamicArray *array, void *item)
{
    return array_push(array, item);
}

void *array_dequeue(DynamicArray *array)
{
    if (array == NULL || array->count == 0 || array->data == NULL)
        return NULL;

    void *item = array->data[array->head];
    size_t next_head = array->head + 1;
    if (next_head >= array->capacity)
        next_head = 0;

    array->head = next_head;
    array->count--;
    return item;
}

void array_free(DynamicArray *array)
{
    if (array == NULL)
        return;

    if (array->data != NULL)
    {
        free(array->data);
        array->data = NULL;
    }

    array->head = 0;
    array->count = 0;
    array->capacity = 0;
}
