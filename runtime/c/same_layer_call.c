/*
 * SPDX-License-Identifier: Apache-2.0
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
