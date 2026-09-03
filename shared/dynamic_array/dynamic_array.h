/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: shared/dynamic_array/dynamic_array.h
 *
 * Purpose:
 *   Declares the heap-backed circular dynamic-array utility and its API.
 *
 * Design:
 *   The contract permits allocation during initialization and growth, so it is
 *   unsuitable for Lettuce hot paths with a zero-allocation requirement.
 */

#ifndef LETTUCE_DYNAMIC_ARRAY_H
#define LETTUCE_DYNAMIC_ARRAY_H

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ARRAY_INITIAL_CAPACITY 4u
#define ARRAY_GROWTH_FACTOR 2u

typedef struct DynamicArray
{
    size_t head;     /* Index of the first element in the circular buffer */
    size_t count;    /* Current number of elements in the array */
    size_t capacity; /* Maximum elements the buffer can hold before resizing */
    void **data;     /* Pointer to array of void pointers (elements) */
} DynamicArray;

/**
 * @brief Initializes a DynamicArray instance with default starting capacity.
 *
 * @param array Pointer to an existing DynamicArray structure to initialize.
 * @return DynamicArray* Pointer to the initialized array, or NULL on allocation failure.
 */
DynamicArray *array_init(DynamicArray *array);

/**
 * @brief Retrieves the item at the specified logical index in the dynamic array.
 *
 * @param array Pointer to the DynamicArray instance.
 * @param index Zero-based logical index (0 <= index < count).
 * @return void* Pointer to item, or NULL if out of bounds or array is NULL.
 */
void *array_get(const DynamicArray *array, size_t index);

/**
 * @brief Sets the item at the specified logical index in the dynamic array.
 *
 * @param array Pointer to the DynamicArray instance.
 * @param index Zero-based logical index.
 * @param item Generic pointer to store.
 * @return bool True on success, false if array is NULL or index is out of bounds.
 */
bool array_set(DynamicArray *array, size_t index, void *item);

/**
 * @brief Appends a new item to the end of the dynamic array.
 *
 * @param array Pointer to the DynamicArray instance.
 * @param item Generic pointer to store.
 * @return bool True on success, false on allocation failure or invalid argument.
 */
bool array_push(DynamicArray *array, void *item);

/**
 * @brief Removes and returns the last item added to the array (LIFO).
 *
 * @param array Pointer to the DynamicArray instance.
 * @return void* Pointer to the popped item, or NULL if the array is empty or NULL.
 */
void *array_pop(DynamicArray *array);

/**
 * @brief Adds a new item to the end of the circular buffer (FIFO enqueue).
 *
 * @param array Pointer to the DynamicArray instance.
 * @param item Generic pointer to store.
 * @return bool True on success, false on allocation failure or invalid argument.
 */
bool array_enqueue(DynamicArray *array, void *item);

/**
 * @brief Removes and returns the first item added to the array (FIFO dequeue).
 *
 * @param array Pointer to the DynamicArray instance.
 * @return void* Pointer to the dequeued item, or NULL if the array is empty or NULL.
 */
void *array_dequeue(DynamicArray *array);

/**
 * @brief Releases memory allocated for internal element storage and resets fields.
 *
 * @param array Pointer to the DynamicArray instance to clean up.
 */
void array_free(DynamicArray *array);

#endif /* LETTUCE_DYNAMIC_ARRAY_H */
