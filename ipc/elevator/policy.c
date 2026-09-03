/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: ipc/elevator/policy.c
 *
 * Purpose:
 *   Performs C-side authorization and target resolution for Elevator calls.
 *
 * Key invariants:
 *   - The caller is derived from current execution state.
 *   - Elevator requests require both CALL and CRITICAL permissions.
 *   - Assembly transition code cannot bypass this policy.
 */

#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"

LettuceStatus lettuce_elevator_policy(const LettuceCallMessage *message, LettuceCallResolution *resolution)
{
    if (message == NULL || resolution == NULL)
        return LETTUCE_STATUS_INVALID_ARGUMENT;

    const LettuceServiceDescriptor *caller = lettuce_service_registry_lookup(current_service_id());
    const LettuceServiceDescriptor *target = lettuce_service_registry_lookup(message->target_service_id);
    if (caller == NULL || target == NULL)
        return LETTUCE_STATUS_INVALID_SERVICE;
    if ((caller->flags & LETTUCE_SERVICE_FLAG_ACTIVE) == 0u ||
        (target->flags & LETTUCE_SERVICE_FLAG_ACTIVE) == 0u)
        return LETTUCE_STATUS_INACTIVE_SERVICE;
    if (message->operation_id == LETTUCE_OPERATION_ID_INVALID)
        return LETTUCE_STATUS_INVALID_OPERATION;
    if (message->resource_id == LETTUCE_RESOURCE_ID_INVALID)
        return LETTUCE_STATUS_INVALID_RESOURCE;
    if (!lettuce_capability_check(message->capability_handle, message->target_service_id,
                                  message->operation_id, LETTUCE_CAP_CALL | LETTUCE_CAP_CRITICAL,
                                  message->resource_id))
        return LETTUCE_STATUS_CAPABILITY_DENIED;

    const LettuceDispatchEntry *entry = lettuce_dispatch_lookup(message->target_service_id, message->operation_id);
    if (entry == NULL || entry->entry == NULL)
        return LETTUCE_STATUS_INVALID_TARGET_ENTRY;

    resolution->caller = caller;
    resolution->target = target;
    resolution->entry = entry;

    return LETTUCE_STATUS_OK;
}
