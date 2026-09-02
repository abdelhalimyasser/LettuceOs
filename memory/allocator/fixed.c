/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>

#include "../../include/lettuce/memory.h"

typedef struct
{
    uint16_t first_page;
    uint16_t page_count;
    LettuceDomainId domain;
    bool active;
} Allocation;

static uint8_t memory_pool[LETTUCE_MEMORY_POOL_SIZE];
static Allocation allocations[LETTUCE_MEMORY_POOL_SIZE / LETTUCE_MEMORY_PAGE_SIZE];

LettuceStatus lettuce_memory_init(void)
{
    for (size_t i = 0; i < sizeof(allocations) / sizeof(allocations[0]); ++i)
        allocations[i] = (Allocation){0};
    return LETTUCE_STATUS_OK;
}

LettuceStatus lettuce_memory_allocate(LettuceDomainId domain, size_t size, LettuceMemoryHandle *handle)
{
    if (domain == LETTUCE_DOMAIN_ID_INVALID || size == 0u || handle == NULL)
        return LETTUCE_STATUS_INVALID_ARGUMENT;

    const size_t pages = (size + LETTUCE_MEMORY_PAGE_SIZE - 1u) / LETTUCE_MEMORY_PAGE_SIZE;
    if (pages > UINT16_MAX)
        return LETTUCE_STATUS_OVERFLOW;

    for (size_t start = 0; start + pages <= sizeof(allocations) / sizeof(allocations[0]); ++start)
    {
        bool free = true;
        for (size_t page = 0; page < pages; ++page)
            free &= !allocations[start + page].active;
        if (!free)
            continue;

        const Allocation allocation = {(uint16_t)start, (uint16_t)pages, domain, true};
        for (size_t page = 0; page < pages; ++page)
            allocations[start + page] = allocation;
        *handle = (LettuceMemoryHandle)(start + 1u);
        return LETTUCE_STATUS_OK;
    }

    return LETTUCE_STATUS_UNAVAILABLE;
}

LettuceStatus lettuce_memory_release(LettuceMemoryHandle handle, LettuceDomainId domain)
{
    if (handle == LETTUCE_MEMORY_HANDLE_INVALID || domain == LETTUCE_DOMAIN_ID_INVALID)
        return LETTUCE_STATUS_INVALID_ARGUMENT;
    const size_t index = handle - 1u;
    if (index >= sizeof(allocations) / sizeof(allocations[0]) || !allocations[index].active)
        return LETTUCE_STATUS_NOT_FOUND;
    if (allocations[index].domain != domain)
        return LETTUCE_STATUS_DENIED;
    const size_t first_page = allocations[index].first_page;
    for (size_t page = 0; page < allocations[index].page_count; ++page)
        allocations[first_page + page] = (Allocation){0};
    return LETTUCE_STATUS_OK;
}

void *lettuce_memory_data(LettuceMemoryHandle handle, LettuceDomainId domain)
{
    if (handle == LETTUCE_MEMORY_HANDLE_INVALID || domain == LETTUCE_DOMAIN_ID_INVALID)
        return NULL;
    const size_t index = handle - 1u;
    if (index >= sizeof(allocations) / sizeof(allocations[0]) || !allocations[index].active || allocations[index].domain != domain)
        return NULL;
    return &memory_pool[allocations[index].first_page * LETTUCE_MEMORY_PAGE_SIZE];
}