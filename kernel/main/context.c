/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../include/kernel.h"

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
