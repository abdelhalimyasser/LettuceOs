/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: tests/unit/dynamic_array_unit.c
 *
 * Purpose:
 *   Host-side unit tests for the non-hot-path dynamic-array utility.
 *
 * Success condition:
 *   Circular ordering, growth, lookup, and cleanup retain the documented API
 *   behavior.
 */

#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#include "../../shared/dynamic_array/dynamic_array.h"

int main(void)
{
    DynamicArray arr;

    /* 1. NULL parameter resilience */
    assert(array_init(NULL) == NULL);
    assert(array_get(NULL, 0) == NULL);
    assert(!array_set(NULL, 0, (void *)0x1));
    assert(!array_push(NULL, (void *)0x1));
    assert(array_pop(NULL) == NULL);
    assert(array_dequeue(NULL) == NULL);
    array_free(NULL);

    /* 2. Initialization */
    assert(array_init(&arr) == &arr);
    assert(arr.count == 0);
    assert(arr.head == 0);
    assert(arr.capacity == ARRAY_INITIAL_CAPACITY);
    assert(arr.data != NULL);

    /* 3. Empty operations */
    assert(array_get(&arr, 0) == NULL);
    assert(array_pop(&arr) == NULL);
    assert(array_dequeue(&arr) == NULL);
    assert(!array_set(&arr, 0, (void *)0x10));

    /* 4. Push and Pop (LIFO) */
    uintptr_t val1 = 100, val2 = 200;
    assert(array_push(&arr, (void *)val1));
    assert(array_push(&arr, (void *)val2));
    assert(arr.count == 2);
    assert((uintptr_t)array_get(&arr, 0) == 100);
    assert((uintptr_t)array_get(&arr, 1) == 200);
    assert(array_get(&arr, 2) == NULL);

    assert((uintptr_t)array_pop(&arr) == 200);
    assert(arr.count == 1);
    assert((uintptr_t)array_pop(&arr) == 100);
    assert(arr.count == 0);
    assert(array_pop(&arr) == NULL);

    /* 5. Circular Enqueue and Dequeue (FIFO) */
    for (uintptr_t i = 1; i <= 3; ++i)
        assert(array_enqueue(&arr, (void *)i));
    assert(arr.count == 3);

    /* Dequeue 2 items: head advances circularly */
    assert((uintptr_t)array_dequeue(&arr) == 1);
    assert((uintptr_t)array_dequeue(&arr) == 2);
    assert(arr.count == 1);
    assert(arr.head == 2);

    /* Enqueue more items to wrap around without resizing yet */
    assert(array_enqueue(&arr, (void *)4));
    assert(array_enqueue(&arr, (void *)5));
    assert(arr.count == 3);

    /* 6. Growth & Resize with circular head != 0 */
    /* Array has capacity 4, count 3, head 2. Pushing 2 more elements triggers resize. */
    assert(array_push(&arr, (void *)6));
    assert(arr.count == 4);
    assert(array_push(&arr, (void *)7));
    assert(arr.count == 5);
    assert(arr.capacity == 8); /* Grown from 4 to 8 */
    assert(arr.head == 0);     /* Preserved logical ordering starting at head 0 */

    /* Verify logical ordering: 3, 4, 5, 6, 7 */
    assert((uintptr_t)array_get(&arr, 0) == 3);
    assert((uintptr_t)array_get(&arr, 1) == 4);
    assert((uintptr_t)array_get(&arr, 2) == 5);
    assert((uintptr_t)array_get(&arr, 3) == 6);
    assert((uintptr_t)array_get(&arr, 4) == 7);
    assert(array_get(&arr, 5) == NULL);

    /* 7. array_set */
    assert(array_set(&arr, 2, (void *)555));
    assert((uintptr_t)array_get(&arr, 2) == 555);

    /* 8. Dequeue all and verify */
    assert((uintptr_t)array_dequeue(&arr) == 3);
    assert((uintptr_t)array_dequeue(&arr) == 4);
    assert((uintptr_t)array_dequeue(&arr) == 555);
    assert((uintptr_t)array_dequeue(&arr) == 6);
    assert((uintptr_t)array_dequeue(&arr) == 7);
    assert(arr.count == 0);
    assert(array_dequeue(&arr) == NULL);

    /* 9. array_free */
    array_free(&arr);
    assert(arr.data == NULL);
    assert(arr.count == 0);
    assert(arr.capacity == 0);
    assert(arr.head == 0);

    return 0;
}
