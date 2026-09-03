/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: memory/allocator/fixed.c
 *
 * Purpose:
 *   Implements the statically bounded page-pool allocator and generational
 *   memory handles.
 *
 * Key invariants:
 *   - Allocation metadata and storage have fixed capacity.
 *   - Handles identify allocation heads and match both generation and domain.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../include/lettuce/memory.h"

#define ALLOC_SLOT_MASK 0xFFFFu
#define ALLOC_GENERATION_SHIFT 16u

typedef struct
{
    uint16_t first_page;
    uint16_t page_count;
    LettuceDomainId domain;
    uint16_t generation;
    bool active;
    uint8_t reserved;
} Allocation;

_Static_assert(sizeof(Allocation) == 12, "Allocation struct must remain 12 bytes.");
_Static_assert(_Alignof(Allocation) == 4, "Allocation struct must be 4-byte aligned.");

static uint8_t memory_pool[LETTUCE_MEMORY_POOL_SIZE];
static Allocation allocations[LETTUCE_MEMORY_POOL_SIZE / LETTUCE_MEMORY_PAGE_SIZE];

static bool decode_handle(LettuceMemoryHandle handle, size_t *first_page, uint16_t *generation)
{
    if (handle == LETTUCE_MEMORY_HANDLE_INVALID || (handle & ALLOC_SLOT_MASK) == 0u)
        return false;

    *first_page = (size_t)((handle & ALLOC_SLOT_MASK) - 1u);
    *generation = (uint16_t)(handle >> ALLOC_GENERATION_SHIFT);
    return *first_page < (sizeof(allocations) / sizeof(allocations[0]));
}

LettuceStatus lettuce_memory_init(void)
{
    const size_t total_pages = sizeof(allocations) / sizeof(allocations[0]);
    for (size_t i = 0; i < total_pages; ++i)
    {
        const uint16_t gen = allocations[i].generation == 0u ? 1u : allocations[i].generation;
        allocations[i] = (Allocation){.generation = gen};
    }
    return LETTUCE_STATUS_OK;
}

LettuceStatus lettuce_memory_allocate(LettuceDomainId domain, size_t size, LettuceMemoryHandle *handle)
{
    if (domain == LETTUCE_DOMAIN_ID_INVALID || size == 0u || handle == NULL)
        return LETTUCE_STATUS_INVALID_ARGUMENT;

    const size_t pages = (size + LETTUCE_MEMORY_PAGE_SIZE - 1u) / LETTUCE_MEMORY_PAGE_SIZE;
    if (pages > UINT16_MAX)
        return LETTUCE_STATUS_OVERFLOW;

    const size_t total_pages = sizeof(allocations) / sizeof(allocations[0]);
    for (size_t start = 0; start + pages <= total_pages; ++start)
    {
        bool free_span = true;
        for (size_t page = 0; page < pages; ++page)
        {
            if (allocations[start + page].active)
            {
                free_span = false;
                break;
            }
        }
        if (!free_span)
            continue;

        uint16_t gen = allocations[start].generation;
        if (gen == 0u)
            gen = 1u;

        const Allocation alloc = {
            .first_page = (uint16_t)start,
            .page_count = (uint16_t)pages,
            .domain = domain,
            .generation = gen,
            .active = true,
            .reserved = 0
        };

        for (size_t page = 0; page < pages; ++page)
            allocations[start + page] = alloc;

        *handle = (LettuceMemoryHandle)(((uint32_t)gen << ALLOC_GENERATION_SHIFT) | (uint32_t)(start + 1u));
        return LETTUCE_STATUS_OK;
    }

    return LETTUCE_STATUS_UNAVAILABLE;
}

LettuceStatus lettuce_memory_release(LettuceMemoryHandle handle, LettuceDomainId domain)
{
    if (domain == LETTUCE_DOMAIN_ID_INVALID)
        return LETTUCE_STATUS_INVALID_ARGUMENT;

    size_t start = 0;
    uint16_t generation = 0;
    if (!decode_handle(handle, &start, &generation))
        return LETTUCE_STATUS_INVALID_ARGUMENT;

    if (!allocations[start].active || allocations[start].generation != generation)
        return LETTUCE_STATUS_NOT_FOUND;

    /* Reject interior page handles: only the head of the allocation is a valid handle */
    if (allocations[start].first_page != start)
        return LETTUCE_STATUS_NOT_FOUND;

    if (allocations[start].domain != domain)
        return LETTUCE_STATUS_DENIED;

    const size_t count = allocations[start].page_count;
    uint16_t next_gen = (uint16_t)(allocations[start].generation + 1u);
    if (next_gen == 0u)
        next_gen = 1u;

    for (size_t page = 0; page < count; ++page)
    {
        allocations[start + page] = (Allocation){
            .generation = next_gen,
            .active = false
        };
    }

    return LETTUCE_STATUS_OK;
}

void *lettuce_memory_data(LettuceMemoryHandle handle, LettuceDomainId domain)
{
    if (domain == LETTUCE_DOMAIN_ID_INVALID)
        return NULL;

    size_t start = 0;
    uint16_t generation = 0;
    if (!decode_handle(handle, &start, &generation))
        return NULL;

    if (!allocations[start].active || allocations[start].generation != generation)
        return NULL;

    /* Reject interior page handles */
    if (allocations[start].first_page != start)
        return NULL;

    if (allocations[start].domain != domain)
        return NULL;

    return &memory_pool[start * LETTUCE_MEMORY_PAGE_SIZE];
}
