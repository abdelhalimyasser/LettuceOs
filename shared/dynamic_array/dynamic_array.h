#ifndef DYNAMIC_ARRAY_H_INCLUDED
#define DYNAMIC_ARRAY_H_INCLUDED

#include <stddef.h>

#define ARRAY_INITIAL_CAPACITY 4
#define ARRAY_GROWTH_FACTOR 2

typedef struct DynamicArray
{
    size_t head;     // Index of the first element in the array
    size_t count;    // Current number of elements in the array
    size_t capacity; // Maximum number of elements the array can hold before resizing
    void **data;     // Pointer to the array of void pointers (generic data)
} DynamicArray;

/**
 * @brief Initializes the dynamic array with a default starting capacity.
 *
 * @details Allocates the initial memory for the array's data pointer and resets
 * the count, head, and capacity counters to their default states.
 *
 * @par Complexity:
 * - **Time Complexity:** O(1)
 * - **Space Complexity:** O(1) (Allocates a fixed initial capacity).
 *
 * @param array Pointer to the DynamicArray instance to initialize.
 * @return void* Pointer to the initialized array, or NULL if memory allocation fails.
 * @note This must be called before using any other array_* functions to avoid uninitialized pointer access.
 */
void *array_init(DynamicArray *array);

/**
 * @brief Retrieves the item at the specified index in the dynamic array.
 *
 * @details Safely accesses the array using circular buffer logic. If the index is out of bounds
 * (greater than or equal to count), this function returns NULL.
 *
 * @par Complexity:
 * - **Time Complexity:** O(1)
 * - **Space Complexity:** O(1)
 *
 * @param array Pointer to the DynamicArray instance.
 * @param index The zero-based index of the item to retrieve.
 * @return void* A pointer to the item at the specified index, or NULL if the index is invalid.
 * @note The caller is responsible for ensuring the index is within bounds to avoid undefined behavior.
 */
void *array_get(DynamicArray *array, size_t index);

/**
 * @brief Sets the item at the specified index in the dynamic array.
 *
 * @details Overwrites the existing generic pointer at the specified position using circular mapping.
 * If the index is out of bounds (greater than or equal to count), this function does nothing.
 *
 * @par Complexity:
 * - **Time Complexity:** O(1)
 * - **Space Complexity:** O(1)
 *
 * @param array Pointer to the DynamicArray instance.
 * @param index The zero-based index of the item to set.
 * @param item A generic void pointer to the data being stored.
 * @return void
 * @note The caller is responsible for ensuring the index is within bounds to avoid memory corruption.
 */
void array_set(DynamicArray *array, size_t index, void *item);

/**
 * @brief Appends a new item to the end of the dynamic array.
 *
 * @details Automatically multiplies the capacity by the growth factor and reallocates
 * memory if the array is full (count == capacity) before inserting the new element.
 *
 * @par Complexity:
 * - **Time Complexity:** Amortized O(1) (Worst case O(N) when resizing occurs).
 * - **Space Complexity:** O(1) (Amortized).
 *
 * @param array Pointer to the DynamicArray instance.
 * @param item A generic void pointer to the data being stored.
 * @return void
 * @note If memory reallocation fails during a resize, the system handles the failure internally (e.g., via exit).
 */
void array_push(DynamicArray *array, void *item);

/**
 * @brief Removes and returns the last item added to the array.
 *
 * @details Decrements the internal count without shrinking the allocated memory,
 * allowing subsequent pushes to be extremely fast. Operates like a Stack (LIFO).
 *
 * @par Complexity:
 * - **Time Complexity:** O(1)
 * - **Space Complexity:** O(1)
 *
 * @param array Pointer to the DynamicArray instance.
 * @return void* A pointer to the popped item, or NULL if the array is empty.
 */
void *array_pop(DynamicArray *array);

/**
 * @brief Adds a new item to the end of the dynamic array.
 *
 * @details Functions identically to array_push, adding the element to the logical tail
 * of the circular buffer. Operates like enqueueing in a Queue (FIFO).
 *
 * @par Complexity:
 * - **Time Complexity:** Amortized O(1).
 * - **Space Complexity:** O(1).
 *
 * @param array Pointer to the DynamicArray instance.
 * @param item A generic void pointer to the data being stored.
 * @return void
 */
void array_enqueue(DynamicArray *array, void *item);

/**
 * @brief Removes and returns the first item added to the array.
 *
 * @details Advances the internal head pointer circularly to virtually remove the item at index 0.
 * This avoids any physical shifting of elements in memory, maximizing performance. Operates like dequeueing in a Queue (FIFO).
 *
 * @par Complexity:
 * - **Time Complexity:** O(1)
 * - **Space Complexity:** O(1).
 *
 * @param array Pointer to the DynamicArray instance.
 * @return void* A pointer to the dequeued item, or NULL if the array is empty.
 */
void *array_dequeue(DynamicArray *array);

/**
 * @brief Frees all memory allocated for the dynamic array's internal data.
 *
 * @details Safely releases the memory array back to the operating system and resets
 * the count, head, and capacity to zero, preventing memory leaks.
 *
 * @par Complexity:
 * - **Time Complexity:** O(1)
 * - **Space Complexity:** O(1)
 *
 * @param array Pointer to the DynamicArray instance to clean up.
 * @return void
 * @note This only frees the array structure itself. If the stored void pointers
 *       point to dynamically allocated memory, they must be freed manually beforehand.
 */
void array_free(DynamicArray *array);

#endif // DYNAMIC_ARRAY_H_INCLUDED