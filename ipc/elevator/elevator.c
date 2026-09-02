/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../../kernel/include/kernel.h"
#include "../../kernel/include/context.h"
#include "../../kernel/include/protection.h"

LettuceStatus lettuce_elevator_gate(const LettuceCallMessage *message)
{
    LettuceCallResolution resolution;
    const LettuceStatus status = lettuce_elevator_policy(message, &resolution);
    if (status != LETTUCE_STATUS_OK)
        return status;

    const LettuceExecutionContext previous_context =
        lettuce_context_enter(resolution.target->id, resolution.target->domain);
    const LettuceStatus result = resolution.entry->entry();
    lettuce_context_leave(previous_context);
    return result;
}/*
 * SPDX-License-Identifier: Apache-2.0
 */
