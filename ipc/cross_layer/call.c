/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: ipc/cross_layer/call.c
 *
 * Purpose:
 *   Implements Cross-Layer call mediation from a validated message to a
 *   registered target entry.
 *
 * Flow:
 *   Authoritative caller -> capability check -> target lookup -> target
 *   execution context -> target entry -> caller context restoration.
 *
 * Key invariants:
 *   - Caller identity comes from kernel execution state.
 *   - Different layers are a direct mediated transition, not hop-by-hop routing.
 */

#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/context.h"
#include "../../kernel/include/protection.h"

LettuceStatus lettuce_cross_layer_validate(const LettuceCallMessage *message, LettuceCallResolution *resolution)
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
	LettuceCallResolution resolution;
	const LettuceStatus status = lettuce_cross_layer_validate(message, &resolution);
	if (status != LETTUCE_STATUS_OK)
		return status;

	const LettuceExecutionContext previous_context =
		lettuce_context_enter(resolution.target->id, resolution.target->domain);
	const LettuceStatus result = resolution.entry->entry();
	lettuce_context_leave(previous_context);
	
	return result;
}
