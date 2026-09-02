/*
 * SPDX-License-Identifier: Apache-2.0
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
    assert(lettuce_shared_buffer_revoke(buffer) == LETTUCE_STATUS_OK);
    assert(lettuce_shared_buffer_access(buffer, read_cap, false, &data, &size) == LETTUCE_STATUS_NOT_FOUND);
    return 0;
}