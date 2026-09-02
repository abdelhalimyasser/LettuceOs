/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>

#include "../../kernel/include/kernel.h"
#include "../../kernel/include/context.h"
#include "../../kernel/include/protection.h"

LettuceStatus lettuce_same_layer_gate(
    LettuceServiceId target_service_id,
    LettuceOperationId operation_id,
    LettuceResourceId resource_id,
    LettuceCapabilityHandle capability_handle)
{
    LettuceCallResolution resolution;
    const LettuceStatus validation_status = lettuce_same_layer_validate(
        target_service_id,
        operation_id,
        resource_id,
        capability_handle,
        &resolution);

    if (validation_status != LETTUCE_STATUS_OK)
        return validation_status;

    if (resolution.entry == NULL || resolution.entry->entry == NULL)
        return LETTUCE_STATUS_INVALID_TARGET_ENTRY;

    const LettuceExecutionContext previous_context =
        lettuce_context_enter(resolution.target->id, resolution.target->domain);
    const LettuceStatus result = resolution.entry->entry();
    lettuce_context_leave(previous_context);

    return result;
}
