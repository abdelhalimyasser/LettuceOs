/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: runtime/c/same_layer_call.c
 *
 * Purpose:
 *   Provides the C runtime wrapper for Same-Layer mediated calls.
 *
 * Flow:
 *   EL0 request -> supervisor caller and layer validation -> capability check
 *   -> target dispatch.
 */

#include <stddef.h>

#include "../../kernel/include/kernel.h"
#include "../../kernel/include/protection.h"
#include "../../include/lettuce/errors.h"
#include "../../include/lettuce/service.h"

LettuceStatus lettuce_same_layer_call(
    LettuceServiceId target_service_id,
    LettuceOperationId operation_id,
    LettuceResourceId resource_id,
    LettuceCapabilityHandle capability_handle)
{
    return lettuce_same_layer_gate(target_service_id, operation_id, resource_id, capability_handle);
}
