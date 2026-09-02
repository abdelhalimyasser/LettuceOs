/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../include/kernel.h"
#include "../include/context.h"

static LettuceServiceId g_current_service_id = LETTUCE_SERVICE_ID_INVALID;

LettuceServiceId current_service_id(void)
{
    return g_current_service_id;
}

void set_current_service_id(LettuceServiceId service_id)
{
    g_current_service_id = service_id;
}

void kernel_set_current_service_id(LettuceServiceId service_id)
{
    set_current_service_id(service_id);
}

LettuceExecutionContext lettuce_context_enter(LettuceServiceId target_service_id, LettuceDomainId target_domain_id)
{
    const LettuceExecutionContext previous_context = {
        .service_id = current_service_id(),
        .domain_id = lettuce_protection_current_domain()
    };

    kernel_set_current_service_id(target_service_id);
    (void)lettuce_protection_enter(target_domain_id);
    return previous_context;
}

void lettuce_context_leave(LettuceExecutionContext previous_context)
{
    lettuce_protection_leave(previous_context.domain_id);
    kernel_set_current_service_id(previous_context.service_id);
}
