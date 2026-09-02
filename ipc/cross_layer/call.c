/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/protection.h"

static LettuceStatus validate_cross_layer_call(const LettuceCallMessage *message, LettuceSameLayerResolution *resolution)
{
	if (message == NULL || resolution == NULL)
		return LETTUCE_STATUS_INVALID_ARGUMENT;

	const LettuceServiceId caller_id = current_service_id();
	const LettuceServiceDescriptor *caller = lettuce_service_registry_lookup(caller_id);
	if (caller == NULL)
		return LETTUCE_STATUS_INVALID_SERVICE;
	if ((caller->flags & LETTUCE_SERVICE_FLAG_ACTIVE) == 0u)
		return LETTUCE_STATUS_INACTIVE_SERVICE;

	const LettuceServiceDescriptor *target = lettuce_service_registry_lookup(message->target_service_id);
	if (target == NULL)
		return LETTUCE_STATUS_INVALID_SERVICE;
	if ((target->flags & LETTUCE_SERVICE_FLAG_ACTIVE) == 0u)
		return LETTUCE_STATUS_INACTIVE_SERVICE;
	if (caller->layer == target->layer)
		return LETTUCE_STATUS_INVALID_STATE;
	if (message->operation_id == LETTUCE_OPERATION_ID_INVALID)
		return LETTUCE_STATUS_INVALID_OPERATION;
	if (message->resource_id == LETTUCE_RESOURCE_ID_INVALID)
		return LETTUCE_STATUS_INVALID_RESOURCE;
	if (!lettuce_capability_check(message->capability_handle, message->target_service_id,
								  message->operation_id, LETTUCE_CAP_CALL, message->resource_id))
		return LETTUCE_STATUS_CAPABILITY_DENIED;

	const LettuceDispatchEntry *entry = lettuce_dispatch_lookup(message->target_service_id, message->operation_id);
	if (entry == NULL || entry->entry == NULL)
		return LETTUCE_STATUS_INVALID_TARGET_ENTRY;

	resolution->caller = caller;
	resolution->target = target;
	resolution->entry = entry;
	return LETTUCE_STATUS_OK;
}

LettuceStatus lettuce_cross_layer_gate(const LettuceCallMessage *message)
{
	LettuceSameLayerResolution resolution;
	const LettuceStatus status = validate_cross_layer_call(message, &resolution);
	if (status != LETTUCE_STATUS_OK)
		return status;

	const LettuceDomainId previous_domain = lettuce_protection_enter(resolution.target->domain);
	const LettuceStatus result = resolution.entry->entry();
	lettuce_protection_leave(previous_domain);
	return result;
}
