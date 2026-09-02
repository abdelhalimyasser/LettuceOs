#include <stdlib.h>
#include "dynamic_array.h"

void *array_init(DynamicArray *array)
{
    if (!array)
        return NULL;

    array->head = 0;                          // Initialize the head index to 0
    array->count = 0;                         // Reset the current number of elements to 0
    array->capacity = ARRAY_INITIAL_CAPACITY; // Set the initial capacity to a predefined constant

    array->data = (void **)malloc(array->capacity * sizeof(void *));

    if (!array->data)
        exit(EXIT_FAILURE);

    return array;
}

void *array_get(DynamicArray *array, size_t index)
{
    if (!array || index >= array->count)
        return NULL; // Return NULL if the array is NULL or the index is out of bounds

    size_t actual_index = (array->head + index) % array->capacity; // Calculate the actual index in the underlying data array
    return array->data[actual_index];
}

void array_set(DynamicArray *array, size_t index, void *item)
{
    if (!array || index >= array->count)
        return; // Do nothing if the array is NULL or the index is out of bounds

    size_t actual_index = (array->head + index) % array->capacity;
    array->data[actual_index] = item;
}

static void resize(DynamicArray *array)
{
    if (!array)
        return;

    size_t new_capacity = array->capacity * ARRAY_GROWTH_FACTOR;
    void **new_data = (void **)malloc(new_capacity * sizeof(void *));

    if (!new_data)
        exit(EXIT_FAILURE);

    // Copy existing elements to the new data array, maintaining their logical order
    for (size_t i = 0; i < array->count; i++)
    {
        size_t index = (array->head + i) % array->capacity;
        new_data[i] = array->data[index];
    }

    // Free the old data array
    free(array->data);

    // Update the array structure to point to the new data array and adjust capacity and head index
    array->data = new_data;
    array->capacity = new_capacity;
    array->head = 0;
}

void array_push(DynamicArray *array, void *item)
{
    if (!array)
        array = array_init(array);

    if (!array)
        return;

    if (array->count >= array->capacity)
        resize(array); // Resize the array if it's full

    size_t tail = (array->head + array->count) % array->capacity; // Calculate the index for the new item, accounting for the circular nature of the array

    array->data[tail] = item;
    array->count++;
}

void *array_pop(DynamicArray *array)
{
    if (!array || array->count == 0)
        return NULL;

    size_t tail = (array->head + array->count - 1) % array->capacity; // Calculate the index of the last item, accounting for the circular nature of the array

    void *item = array->data[tail];
    array->count--;

    return item;
}

void array_enqueue(DynamicArray *array, void *item)
{
    if (!array)
        array = array_init(array);

    if (!array)
        return;

    array_push(array, item); // Reuse the push function to add the item to the end
}

void *array_dequeue(DynamicArray *array)
{
    if (!array || array->count == 0)
        return NULL;

    void *item = array->data[array->head];

    array->head = (array->head + 1) % array->capacity; // Move the head index forward
    array->count--;                                    // Decrement the count

    return item; // Return the dequeued item
}

void array_free(DynamicArray *array)
{
    if (!array)
        return;

    if (array->data)
    {
        free(array->data);
        array->data = NULL;
    }

    array->head = 0;
    array->count = 0;
    array->capacity = 0;
}