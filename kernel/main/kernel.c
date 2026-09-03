/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/main/kernel.c
 *
 * Purpose:
 *   Initializes the portable kernel subsystems in their required dependency
 *   order.
 *
 * Flow:
 *   Capability, registry, context, protection, memory, and scheduler setup
 *   -> architecture initialization.
 */

#include <stdbool.h>
#include "../include/kernel.h"

static LettuceServiceRegistryEntry service_table[LETTUCE_SERVICE_TABLE_SIZE];

bool lettuce_service_registry_init(void)
{
    for (uint32_t i = 0; i < LETTUCE_SERVICE_TABLE_SIZE; ++i)
    {
        service_table[i].descriptor = (LettuceServiceDescriptor){0};
        for (uint32_t operation = 0; operation < LETTUCE_DISPATCH_OPERATION_LIMIT; ++operation)
            service_table[i].operations[operation] = (LettuceDispatchEntry){0};
    }

    return true;
}

bool lettuce_service_registry_register(LettuceServiceDescriptor descriptor)
{
    if (descriptor.id == LETTUCE_SERVICE_ID_INVALID || descriptor.id >= LETTUCE_SERVICE_TABLE_SIZE)
        return false;

    if (descriptor.layer > LETTUCE_LAYER_L4)
        return false;

    LettuceServiceRegistryEntry *entry = &service_table[descriptor.id];
    if (entry->descriptor.id != LETTUCE_SERVICE_ID_INVALID)
        return false;

    entry->descriptor = descriptor;
    return true;
}

bool lettuce_service_registry_unregister(LettuceServiceId service_id)
{
    if (service_id == LETTUCE_SERVICE_ID_INVALID || service_id >= LETTUCE_SERVICE_TABLE_SIZE)
        return false;

    LettuceServiceRegistryEntry *entry = &service_table[service_id];
    if (entry->descriptor.id == LETTUCE_SERVICE_ID_INVALID)
        return false;

    entry->descriptor = (LettuceServiceDescriptor){0};
    for (uint32_t operation = 0; operation < LETTUCE_DISPATCH_OPERATION_LIMIT; ++operation)
        entry->operations[operation] = (LettuceDispatchEntry){0};
    return true;
}

const LettuceServiceDescriptor *lettuce_service_registry_lookup(LettuceServiceId service_id)
{
    if (service_id == LETTUCE_SERVICE_ID_INVALID || service_id >= LETTUCE_SERVICE_TABLE_SIZE)
        return NULL;

    const LettuceServiceDescriptor *descriptor = &service_table[service_id].descriptor;
    return descriptor->id == service_id ? descriptor : NULL;
}

const LettuceServiceRegistryEntry *lettuce_service_registry_entry(LettuceServiceId service_id)
{
    if (service_id == LETTUCE_SERVICE_ID_INVALID || service_id >= LETTUCE_SERVICE_TABLE_SIZE)
        return NULL;

    const LettuceServiceRegistryEntry *entry = &service_table[service_id];
    return entry->descriptor.id == service_id ? entry : NULL;
}

LettuceServiceRegistryEntry *lettuce_service_registry_entry_mutable(LettuceServiceId service_id)
{
    if (service_id == LETTUCE_SERVICE_ID_INVALID || service_id >= LETTUCE_SERVICE_TABLE_SIZE)
        return NULL;

    LettuceServiceRegistryEntry *entry = &service_table[service_id];
    return entry->descriptor.id == service_id ? entry : NULL;
}

bool lettuce_service_registry_is_active(LettuceServiceId service_id)
{
    const LettuceServiceDescriptor *descriptor = lettuce_service_registry_lookup(service_id);
    return descriptor != NULL && (descriptor->flags & LETTUCE_SERVICE_FLAG_ACTIVE) != 0u;
}

bool lettuce_service_registry_validate(LettuceServiceId service_id)
{
    return lettuce_service_registry_is_active(service_id);
}

int kernel_main(void)
{
    lettuce_service_registry_init();
    return 0;
}
