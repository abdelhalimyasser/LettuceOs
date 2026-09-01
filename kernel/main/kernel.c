/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <string.h>

#include "../include/kernel.h"

static LettuceServiceRegistryEntry service_table[LETTUCE_SERVICE_TABLE_SIZE];

bool lettuce_service_registry_init(void)
{
    for (uint32_t i = 0; i < LETTUCE_SERVICE_TABLE_SIZE; ++i)
    {
        service_table[i].id = LETTUCE_SERVICE_ID_INVALID;
        service_table[i].descriptor = (LettuceServiceDescriptor){0};
        service_table[i].active = false;
    }

    return true;
}

bool lettuce_service_registry_register(LettuceServiceDescriptor descriptor)
{
    if (descriptor.id == LETTUCE_SERVICE_ID_INVALID)
        return false;

    if (descriptor.layer > LETTUCE_LAYER_L4)
        return false;

    if (lettuce_service_registry_lookup(descriptor.id) != NULL)
        return false;

    for (uint32_t i = 0; i < LETTUCE_SERVICE_TABLE_SIZE; ++i)
    {
        if (service_table[i].active)
            continue;

        service_table[i].id = descriptor.id;
        service_table[i].descriptor = descriptor;
        service_table[i].active = true;
        return true;
    }

    return false;
}

bool lettuce_service_registry_unregister(LettuceServiceId service_id)
{
    for (uint32_t i = 0; i < LETTUCE_SERVICE_TABLE_SIZE; ++i)
    {
        if (!service_table[i].active)
            continue;

        if (service_table[i].id != service_id)
            continue;

        service_table[i].id = LETTUCE_SERVICE_ID_INVALID;
        service_table[i].descriptor = (LettuceServiceDescriptor){0};
        service_table[i].active = false;
        return true;
    }

    return false;
}

const LettuceServiceDescriptor *lettuce_service_registry_lookup(LettuceServiceId service_id)
{
    for (uint32_t i = 0; i < LETTUCE_SERVICE_TABLE_SIZE; ++i)
    {
        if (!service_table[i].active)
            continue;

        if (service_table[i].id == service_id)
            return &service_table[i].descriptor;
    }

    return NULL;
}

bool lettuce_service_registry_is_active(LettuceServiceId service_id)
{
    return lettuce_service_registry_lookup(service_id) != NULL;
}

bool lettuce_service_registry_validate(LettuceServiceId service_id)
{
    const LettuceServiceDescriptor *descriptor = lettuce_service_registry_lookup(service_id);
    if (descriptor == NULL)
        return false;

    return descriptor->id == service_id && descriptor->flags != 0u;
}

int kernel_main(void)
{
    lettuce_service_registry_init();
    return 0;
}
