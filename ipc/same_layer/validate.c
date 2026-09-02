/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>

#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"

LettuceStatus lettuce_same_layer_validate(
    LettuceServiceId target_service_id,
    LettuceOperationId operation_id,
    LettuceResourceId resource_id,
    LettuceCapabilityHandle capability_handle,
    LettuceSameLayerResolution *resolution)
{
    if (resolution == NULL)
        return LETTUCE_STATUS_INVALID_ARGUMENT;

    const LettuceServiceId trusted_caller = current_service_id();
    if (trusted_caller == LETTUCE_SERVICE_ID_INVALID)
        return LETTUCE_STATUS_INVALID_SERVICE;

    const LettuceServiceDescriptor *caller = lettuce_service_registry_lookup(trusted_caller);
    if (caller == NULL)
        return LETTUCE_STATUS_INVALID_SERVICE;

    if ((caller->flags & LETTUCE_SERVICE_FLAG_ACTIVE) == 0u)
        return LETTUCE_STATUS_INACTIVE_SERVICE;

    if (target_service_id == LETTUCE_SERVICE_ID_INVALID)
        return LETTUCE_STATUS_INVALID_SERVICE;

    const LettuceServiceDescriptor *target = lettuce_service_registry_lookup(target_service_id);
    if (target == NULL)
        return LETTUCE_STATUS_INVALID_SERVICE;

    if ((target->flags & LETTUCE_SERVICE_FLAG_ACTIVE) == 0u)
        return LETTUCE_STATUS_INACTIVE_SERVICE;

    if (caller->layer != target->layer)
        return LETTUCE_STATUS_DIFFERENT_LAYER;

    if (operation_id == LETTUCE_OPERATION_ID_INVALID)
        return LETTUCE_STATUS_INVALID_OPERATION;

    if (resource_id == LETTUCE_RESOURCE_ID_INVALID)
        return LETTUCE_STATUS_INVALID_RESOURCE;

    if (capability_handle == LETTUCE_CAPABILITY_INVALID)
        return LETTUCE_STATUS_CAPABILITY_DENIED;

    if (!lettuce_capability_check(capability_handle, target_service_id, operation_id, LETTUCE_CAP_CALL, resource_id))
        return LETTUCE_STATUS_CAPABILITY_DENIED;

    const LettuceDispatchEntry *entry = lettuce_dispatch_lookup(target_service_id, operation_id);
    if (entry == NULL)
        return LETTUCE_STATUS_INVALID_OPERATION;

    resolution->caller = caller;
    resolution->target = target;
    resolution->entry = entry;
    return LETTUCE_STATUS_OK;
}
