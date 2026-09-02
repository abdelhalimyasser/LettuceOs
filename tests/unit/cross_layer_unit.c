/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>

#include "../../include/lettuce/message.h"
#include "../../include/lettuce/service.h"
#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"

static LettuceStatus service_entry(void)
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
    assert(lettuce_dispatch_register(20u, 3u, service_entry));

    set_current_service_id(10u);
    const LettuceCapabilityHandle capability =
        lettuce_capability_create(10u, 20u, 3u, LETTUCE_CAP_CALL, 300u);
    assert(capability != LETTUCE_CAPABILITY_INVALID);

    const LettuceCallMessage message = {20u, 3u, 300u, capability};
    assert(lettuce_cross_layer_call(&message) == LETTUCE_STATUS_OK);

    LettuceCallMessage wrong_resource = message;
    wrong_resource.resource_id = 301u;
    assert(lettuce_cross_layer_call(&wrong_resource) == LETTUCE_STATUS_CAPABILITY_DENIED);

    assert(lettuce_same_layer_call(20u, 3u, 300u, capability) == LETTUCE_STATUS_DIFFERENT_LAYER);
    return 0;
}