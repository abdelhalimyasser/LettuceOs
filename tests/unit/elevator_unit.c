/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>

#include "../../include/lettuce/message.h"
#include "../../include/lettuce/service.h"
#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"

static LettuceStatus critical_entry(void)
{
    return LETTUCE_STATUS_OK;
}

int main(void)
{
    lettuce_capability_init();
    lettuce_service_registry_init();
    assert(lettuce_service_registry_register((LettuceServiceDescriptor){
        .id = 10u, .layer = LETTUCE_LAYER_L3, .domain = 100u,
        .flags = LETTUCE_SERVICE_FLAG_ACTIVE}));
    assert(lettuce_service_registry_register((LettuceServiceDescriptor){
        .id = 20u, .layer = LETTUCE_LAYER_L1, .domain = 200u,
        .flags = LETTUCE_SERVICE_FLAG_ACTIVE}));
    assert(lettuce_dispatch_register(20u, 4u, critical_entry));
    set_current_service_id(10u);

    const LettuceCapabilityHandle normal = lettuce_capability_create(10u, 20u, 4u, LETTUCE_CAP_CALL, 300u);
    const LettuceCapabilityHandle critical = lettuce_capability_create(
        10u, 20u, 4u, LETTUCE_CAP_CALL | LETTUCE_CAP_CRITICAL, 300u);
    const LettuceCallMessage normal_message = {20u, 4u, 300u, normal};
    const LettuceCallMessage critical_message = {20u, 4u, 300u, critical};

    assert(lettuce_elevator_call(&normal_message) == LETTUCE_STATUS_CAPABILITY_DENIED);
    assert(lettuce_elevator_call(&critical_message) == LETTUCE_STATUS_OK);
    assert(lettuce_cross_layer_call(&critical_message) == LETTUCE_STATUS_OK);
    return 0;
}