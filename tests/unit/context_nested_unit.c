/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>

#include "../../include/lettuce/service.h"
#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/context.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/protection.h"

static LettuceCapabilityHandle b_to_c_capability;

static LettuceStatus service_c(void)
{
    assert(current_service_id() == 30u);
    assert(lettuce_protection_current_domain() == 300u);
    return LETTUCE_STATUS_OK;
}

static LettuceStatus service_b(void)
{
    assert(current_service_id() == 20u);
    assert(lettuce_protection_current_domain() == 200u);
    assert(lettuce_same_layer_call(30u, 3u, 300u, b_to_c_capability) == LETTUCE_STATUS_OK);
    assert(current_service_id() == 20u);
    assert(lettuce_protection_current_domain() == 200u);
    return LETTUCE_STATUS_OK;
}

static LettuceStatus failing_service(void)
{
    assert(current_service_id() == 40u);
    assert(lettuce_protection_current_domain() == 400u);
    return LETTUCE_STATUS_ERROR;
}

static void register_service(LettuceServiceId id, LettuceDomainId domain)
{
    assert(lettuce_service_registry_register((LettuceServiceDescriptor){
        .id = id, .layer = LETTUCE_LAYER_L3, .domain = domain,
        .flags = LETTUCE_SERVICE_FLAG_ACTIVE}));
}

int main(void)
{
    lettuce_capability_init();
    lettuce_service_registry_init();
    register_service(10u, 100u);
    register_service(20u, 200u);
    register_service(30u, 300u);
    register_service(40u, 400u);
    assert(lettuce_dispatch_register(20u, 2u, service_b));
    assert(lettuce_dispatch_register(30u, 3u, service_c));
    assert(lettuce_dispatch_register(40u, 4u, failing_service));

    const LettuceCapabilityHandle a_to_b = lettuce_capability_create(10u, 20u, 2u, LETTUCE_CAP_CALL, 200u);
    b_to_c_capability = lettuce_capability_create(20u, 30u, 3u, LETTUCE_CAP_CALL, 300u);
    const LettuceCapabilityHandle a_to_error = lettuce_capability_create(10u, 40u, 4u, LETTUCE_CAP_CALL, 400u);
    assert(a_to_b != LETTUCE_CAPABILITY_INVALID);
    assert(b_to_c_capability != LETTUCE_CAPABILITY_INVALID);
    assert(a_to_error != LETTUCE_CAPABILITY_INVALID);

    const LettuceExecutionContext initial = lettuce_context_enter(10u, 100u);
    assert(current_service_id() == 10u);
    assert(lettuce_protection_current_domain() == 100u);
    assert(lettuce_same_layer_call(20u, 2u, 200u, a_to_b) == LETTUCE_STATUS_OK);
    assert(current_service_id() == 10u);
    assert(lettuce_protection_current_domain() == 100u);
    assert(lettuce_same_layer_call(40u, 4u, 400u, a_to_error) == LETTUCE_STATUS_ERROR);
    assert(current_service_id() == 10u);
    assert(lettuce_protection_current_domain() == 100u);
    lettuce_context_leave(initial);

    assert(current_service_id() == LETTUCE_SERVICE_ID_INVALID);
    assert(lettuce_protection_current_domain() == LETTUCE_DOMAIN_ID_INVALID);
    return 0;
}