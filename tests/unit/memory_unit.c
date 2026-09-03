/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: tests/unit/memory_unit.c
 *
 * Purpose:
 *   Host-side unit tests for fixed memory allocation and shared-buffer access.
 *
 * Success condition:
 *   Bounded storage, domain checks, generation handles, and capability-bound
 *   buffer operations reject invalid access.
 */

#include <assert.h>
#include <stddef.h>

#include "../../include/lettuce/memory.h"
#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"

#define READ_OPERATION LETTUCE_SHARED_READ_OPERATION
#define WRITE_OPERATION LETTUCE_SHARED_WRITE_OPERATION

int main(void)
{
    assert(lettuce_memory_init() == LETTUCE_STATUS_OK);
    LettuceMemoryHandle allocation = LETTUCE_MEMORY_HANDLE_INVALID;
    assert(lettuce_memory_allocate(10u, 5000u, &allocation) == LETTUCE_STATUS_OK);
    assert(lettuce_memory_data(allocation, 10u) != NULL);
    assert(lettuce_memory_data(allocation, 11u) == NULL);
    assert(lettuce_memory_release(allocation, 11u) == LETTUCE_STATUS_DENIED);
    assert(lettuce_memory_release(allocation, 10u) == LETTUCE_STATUS_OK);
    /* Verify stale handle is rejected */
    assert(lettuce_memory_release(allocation, 10u) == LETTUCE_STATUS_NOT_FOUND);
    assert(lettuce_memory_data(allocation, 10u) == NULL);

    /* Test multi-page allocation and interior handle rejection */
    LettuceMemoryHandle multi = LETTUCE_MEMORY_HANDLE_INVALID;
    assert(lettuce_memory_allocate(10u, 8192u, &multi) == LETTUCE_STATUS_OK);
    const uint16_t gen = (uint16_t)(multi >> 16u);
    const uint16_t slot = (uint16_t)(multi & 0xffffu);
    const LettuceMemoryHandle interior_handle = (LettuceMemoryHandle)(((uint32_t)gen << 16u) | (uint32_t)(slot + 1u));
    assert(lettuce_memory_data(interior_handle, 10u) == NULL);
    assert(lettuce_memory_release(interior_handle, 10u) == LETTUCE_STATUS_NOT_FOUND);
    assert(lettuce_memory_release(multi, 10u) == LETTUCE_STATUS_OK);

    /* Test stale handle rejection after re-allocation in same slot */
    LettuceMemoryHandle realloc_handle = LETTUCE_MEMORY_HANDLE_INVALID;
    assert(lettuce_memory_allocate(10u, 4096u, &realloc_handle) == LETTUCE_STATUS_OK);
    assert((realloc_handle & 0xffffu) == (multi & 0xffffu));
    assert(realloc_handle != multi);
    assert(lettuce_memory_data(multi, 10u) == NULL);
    assert(lettuce_memory_release(multi, 10u) == LETTUCE_STATUS_NOT_FOUND);
    assert(lettuce_memory_release(realloc_handle, 10u) == LETTUCE_STATUS_OK);

    lettuce_capability_init();
    lettuce_service_registry_init();
    assert(lettuce_service_registry_register((LettuceServiceDescriptor){
        .id = 10u, .layer = LETTUCE_LAYER_L3, .domain = 10u,
        .flags = LETTUCE_SERVICE_FLAG_ACTIVE}));
    assert(lettuce_service_registry_register((LettuceServiceDescriptor){
        .id = 20u, .layer = LETTUCE_LAYER_L3, .domain = 20u,
        .flags = LETTUCE_SERVICE_FLAG_ACTIVE}));
    set_current_service_id(10u);
    const LettuceCapabilityHandle read_cap = lettuce_capability_create(20u, 10u, READ_OPERATION, LETTUCE_CAP_READ, 700u);
    const LettuceCapabilityHandle write_cap = lettuce_capability_create(20u, 10u, WRITE_OPERATION, LETTUCE_CAP_WRITE, 700u);
    LettuceSharedBufferHandle buffer = LETTUCE_SHARED_BUFFER_INVALID;
    assert(lettuce_shared_buffer_init() == LETTUCE_STATUS_OK);
    assert(lettuce_shared_buffer_create(10u, 700u, 128u, read_cap, write_cap, &buffer) == LETTUCE_STATUS_OK);

    set_current_service_id(20u);
    void *data = NULL;
    size_t size = 0u;
    assert(lettuce_shared_buffer_access(buffer, read_cap, false, &data, &size) == LETTUCE_STATUS_OK);
    assert(data != NULL && size == 128u);
    assert(lettuce_shared_buffer_access(buffer, write_cap, true, &data, &size) == LETTUCE_STATUS_OK);
    assert(lettuce_shared_buffer_revoke(buffer) == LETTUCE_STATUS_DENIED);
    assert(lettuce_shared_buffer_access(buffer, read_cap, false, &data, &size) == LETTUCE_STATUS_OK);
    set_current_service_id(10u);
    assert(lettuce_shared_buffer_revoke(buffer) == LETTUCE_STATUS_OK);
    assert(lettuce_shared_buffer_revoke(buffer) == LETTUCE_STATUS_NOT_FOUND);
    assert(lettuce_shared_buffer_access(buffer, read_cap, false, &data, &size) == LETTUCE_STATUS_NOT_FOUND);

    set_current_service_id(10u);
    LettuceSharedBufferHandle replacement = LETTUCE_SHARED_BUFFER_INVALID;
    assert(lettuce_shared_buffer_create(10u, 700u, 128u, read_cap, write_cap, &replacement) == LETTUCE_STATUS_OK);
    assert((buffer & 0xFFFFu) == (replacement & 0xFFFFu));
    assert((buffer >> 16u) != (replacement >> 16u));
    assert(buffer != replacement);
    assert(lettuce_shared_buffer_access(buffer, read_cap, false, &data, &size) == LETTUCE_STATUS_NOT_FOUND);
    assert(lettuce_shared_buffer_revoke(buffer) == LETTUCE_STATUS_NOT_FOUND);
    assert(lettuce_shared_buffer_revoke(replacement) == LETTUCE_STATUS_OK);
    return 0;
}
