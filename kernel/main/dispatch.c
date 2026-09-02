/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../include/kernel.h"

bool lettuce_dispatch_register(LettuceServiceId service_id, LettuceOperationId operation_id, LettuceStatus (*entry)(void))
{
    if (service_id == LETTUCE_SERVICE_ID_INVALID || operation_id == LETTUCE_OPERATION_ID_INVALID ||
        operation_id >= LETTUCE_DISPATCH_OPERATION_LIMIT || entry == NULL)
        return false;

    if (!lettuce_service_registry_is_active(service_id))
        return false;

    LettuceServiceRegistryEntry *service = lettuce_service_registry_entry_mutable(service_id);
    LettuceDispatchEntry *dispatch = &service->operations[operation_id];
    if (dispatch->active)
        return false;

    dispatch->entry = entry;
    dispatch->active = true;
    return true;
}

bool lettuce_dispatch_unregister(LettuceServiceId service_id, LettuceOperationId operation_id)
{
    if (operation_id == LETTUCE_OPERATION_ID_INVALID || operation_id >= LETTUCE_DISPATCH_OPERATION_LIMIT)
        return false;

    LettuceServiceRegistryEntry *service = lettuce_service_registry_entry_mutable(service_id);
    if (service == NULL || !service->operations[operation_id].active)
        return false;

    service->operations[operation_id].entry = NULL;
    service->operations[operation_id].active = false;
    return true;
}

const LettuceDispatchEntry *lettuce_dispatch_lookup(LettuceServiceId service_id, LettuceOperationId operation_id)
{
    if (operation_id == LETTUCE_OPERATION_ID_INVALID || operation_id >= LETTUCE_DISPATCH_OPERATION_LIMIT)
        return NULL;

    const LettuceServiceRegistryEntry *service = lettuce_service_registry_entry_mutable(service_id);
    if (service == NULL || !service->operations[operation_id].active)
        return NULL;

    return &service->operations[operation_id];
}

