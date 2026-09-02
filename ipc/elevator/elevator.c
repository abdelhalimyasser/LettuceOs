/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../../kernel/include/kernel.h"
#include "../../kernel/include/protection.h"

LettuceStatus lettuce_elevator_gate(const LettuceCallMessage *message)
{
    LettuceSameLayerResolution resolution;
    const LettuceStatus status = lettuce_elevator_policy(message, &resolution);
    if (status != LETTUCE_STATUS_OK)
        return status;

    const LettuceDomainId previous_domain = lettuce_protection_enter(resolution.target->domain);
    const LettuceStatus result = resolution.entry->entry();
    lettuce_protection_leave(previous_domain);
    return result;
}/*
 * SPDX-License-Identifier: Apache-2.0
 */
