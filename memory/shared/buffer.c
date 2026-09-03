/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: memory/shared/buffer.c
 *
 * Purpose:
 *   Implements fixed-capacity shared buffers protected by generational handles
 *   and capability checks.
 *
 * Flow:
 *   Owner create -> capability-bound access -> revoke and generation advance.
 *
 * Key invariants:
 *   - Buffers are zeroed on reuse and revocation.
 *   - Access validates the requested read or write capability.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "../../include/lettuce/memory.h"
#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"

#define SHARED_SLOT_MASK 0xFFFFu
#define SHARED_GENERATION_SHIFT 16u

/*
 * SharedBuffer internal layout:
 * - 32-byte header aligned to 8-byte boundaries.
 * - data payload starts at offset 32 (8-byte aligned, avoiding misaligned payload accesses).
 *
 * NOTE ON RAW POINTER LIFETIME (Host Prototype):
 * In this host userspace prototype, lettuce_shared_buffer_access returns a raw pointer
 * to buffer->data. After revocation, subsequent logical access attempts via the API are
 * rejected with LETTUCE_STATUS_NOT_FOUND. However, in pure userspace execution without
 * hardware MMU page remapping per domain, an already-obtained raw pointer remains in the
 * caller's address space until real hardware page table boundaries are switched.
 */
typedef struct
{
    size_t size;                              /* Offset  0: 8 bytes */
    LettuceResourceId resource;               /* Offset  8: 4 bytes */
    LettuceCapabilityHandle read_capability;  /* Offset 12: 4 bytes */
    LettuceCapabilityHandle write_capability; /* Offset 16: 4 bytes */
    LettuceServiceId owner;                   /* Offset 20: 4 bytes */
    uint16_t generation;                      /* Offset 24: 2 bytes */
    bool active;                              /* Offset 26: 1 byte  */
    uint8_t reserved[5];                      /* Offset 27: 5 bytes explicit alignment pad */
    uint8_t data[LETTUCE_SHARED_BUFFER_SIZE]; /* Offset 32: 4096 bytes (8-byte aligned) */
} SharedBuffer;

_Static_assert(sizeof(SharedBuffer) == (32 + LETTUCE_SHARED_BUFFER_SIZE),
               "SharedBuffer header must remain exactly 32 bytes.");
_Static_assert(offsetof(SharedBuffer, data) == 32,
               "SharedBuffer.data must start at 8-byte aligned offset 32.");

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

    const LettuceServiceId caller = current_service_id();
    if (caller != owner)
        return LETTUCE_STATUS_DENIED;

    for (size_t i = 0; i < LETTUCE_SHARED_BUFFER_COUNT; ++i)
    {
        if (buffers[i].active)
            continue;

        uint16_t gen = buffers[i].generation;
        if (gen == 0u)
            gen = 1u;

        buffers[i].owner = owner;
        buffers[i].resource = resource;
        buffers[i].size = size;
        buffers[i].read_capability = read_capability;
        buffers[i].write_capability = write_capability;
        buffers[i].generation = gen;
        buffers[i].active = true;

        /* Zero payload on reuse to prevent cross-service information leakage */
        for (size_t b = 0; b < sizeof(buffers[i].data); ++b)
            buffers[i].data[b] = 0;

        *handle = (LettuceSharedBufferHandle)(((uint32_t)gen << SHARED_GENERATION_SHIFT) | (uint32_t)(i + 1u));
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

    if (current_service_id() != buffer->owner)
        return LETTUCE_STATUS_DENIED;

    buffer->active = false;
    uint16_t next_gen = (uint16_t)(buffer->generation + 1u);
    if (next_gen == 0u)
        next_gen = 1u;
    buffer->generation = next_gen;

    /* Zero payload on revoke */
    for (size_t b = 0; b < sizeof(buffer->data); ++b)
        buffer->data[b] = 0;

    return LETTUCE_STATUS_OK;
}
