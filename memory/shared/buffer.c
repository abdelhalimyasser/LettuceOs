/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include "../../include/lettuce/memory.h"
#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"

#define SHARED_SLOT_MASK 0xFFFFu
#define SHARED_GENERATION_SHIFT 16u
typedef struct
{
    LettuceServiceId owner;
    LettuceResourceId resource;
    size_t size;
    LettuceCapabilityHandle read_capability;
    LettuceCapabilityHandle write_capability;
    uint16_t generation;
    bool active;
    uint8_t data[LETTUCE_SHARED_BUFFER_SIZE];
} SharedBuffer;

static SharedBuffer buffers[LETTUCE_SHARED_BUFFER_COUNT];

static bool decode_handle(LettuceSharedBufferHandle handle, size_t *slot, uint16_t *generation)
{
    if (handle == LETTUCE_SHARED_BUFFER_INVALID || (handle & SHARED_SLOT_MASK) == 0u)
        return false;
    *slot = (size_t)((handle & SHARED_SLOT_MASK) - 1u);
    *generation = (uint16_t)(handle >> SHARED_GENERATION_SHIFT);
    return *slot < LETTUCE_SHARED_BUFFER_COUNT;
}

LettuceStatus lettuce_shared_buffer_init(void)
{
    for (size_t i = 0; i < LETTUCE_SHARED_BUFFER_COUNT; ++i)
    {
        const uint16_t generation = buffers[i].generation == 0u ? 1u : buffers[i].generation;
        buffers[i] = (SharedBuffer){.generation = generation};
    }
    return LETTUCE_STATUS_OK;
}

LettuceStatus lettuce_shared_buffer_create(
    LettuceServiceId owner,
    LettuceResourceId resource,
    size_t size,
    LettuceCapabilityHandle read_capability,
    LettuceCapabilityHandle write_capability,
    LettuceSharedBufferHandle *handle)
{
    if (owner == LETTUCE_SERVICE_ID_INVALID || resource == LETTUCE_RESOURCE_ID_INVALID ||
        size == 0u || size > LETTUCE_SHARED_BUFFER_SIZE || handle == NULL)
        return LETTUCE_STATUS_INVALID_ARGUMENT;
    if (current_service_id() != owner)
        return LETTUCE_STATUS_DENIED;

    for (size_t i = 0; i < LETTUCE_SHARED_BUFFER_COUNT; ++i)
    {
        if (buffers[i].active)
            continue;
        if (buffers[i].generation == 0u)
            buffers[i].generation = 1u;
        buffers[i].owner = owner;
        buffers[i].resource = resource;
        buffers[i].size = size;
        buffers[i].read_capability = read_capability;
        buffers[i].write_capability = write_capability;
        buffers[i].active = true;
        *handle = (LettuceSharedBufferHandle)(((uint32_t)buffers[i].generation << SHARED_GENERATION_SHIFT) | (uint32_t)(i + 1u));
        return LETTUCE_STATUS_OK;
    }
    return LETTUCE_STATUS_UNAVAILABLE;
}

LettuceStatus lettuce_shared_buffer_access(
    LettuceSharedBufferHandle handle,
    LettuceCapabilityHandle capability,
    bool write,
    void **data,
    size_t *size)
{
    size_t slot = 0u;
    uint16_t generation = 0u;
    if (data == NULL || size == NULL || !decode_handle(handle, &slot, &generation))
        return LETTUCE_STATUS_INVALID_ARGUMENT;
    SharedBuffer *buffer = &buffers[slot];
    if (!buffer->active || buffer->generation != generation)
        return LETTUCE_STATUS_NOT_FOUND;

    const LettuceOperationId operation = write ? LETTUCE_SHARED_WRITE_OPERATION : LETTUCE_SHARED_READ_OPERATION;
    const LettuceCapabilityHandle expected = write ? buffer->write_capability : buffer->read_capability;
    if (capability != expected ||
        !lettuce_capability_check(capability, buffer->owner, operation,
                                  write ? LETTUCE_CAP_WRITE : LETTUCE_CAP_READ, buffer->resource))
        return LETTUCE_STATUS_CAPABILITY_DENIED;

    *data = buffer->data;
    *size = buffer->size;
    return LETTUCE_STATUS_OK;
}

LettuceStatus lettuce_shared_buffer_revoke(LettuceSharedBufferHandle handle)
{
    size_t slot = 0u;
    uint16_t generation = 0u;
    if (!decode_handle(handle, &slot, &generation))
        return LETTUCE_STATUS_INVALID_ARGUMENT;
    SharedBuffer *buffer = &buffers[slot];
    if (!buffer->active || buffer->generation != generation)
        return LETTUCE_STATUS_NOT_FOUND;
    buffer->active = false;
    buffer->generation = (uint16_t)(buffer->generation + 1u);
    if (buffer->generation == 0u)
        buffer->generation = 1u;
    return LETTUCE_STATUS_OK;
}