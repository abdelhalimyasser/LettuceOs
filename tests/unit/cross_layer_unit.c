/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: tests/unit/cross_layer_unit.c
 *
 * Purpose:
 *   Host-side unit tests for Cross-Layer capability-mediated dispatch.
 *
 * Success condition:
 *   Different-layer services are reached only through validation and the
 *   caller context remains authoritative throughout the transition.
 */

#include <assert.h>

#include "../../include/lettuce/message.h"
#include "../../include/lettuce/service.h"
#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/protection.h"

static LettuceStatus service_entry(void)
{
    return LETTUCE_STATUS_OK;
}

static LettuceStatus failing_entry(void)
{
    assert(current_service_id() == 20u);
    assert(lettuce_protection_current_domain() == 200u);
    return LETTUCE_STATUS_ERROR;
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
    assert(lettuce_service_registry_register((LettuceServiceDescriptor){
        .id = 30u, .layer = LETTUCE_LAYER_L2, .domain = 300u,
        .flags = 0u}));
    assert(lettuce_dispatch_register(20u, 3u, service_entry));
    assert(lettuce_dispatch_register(20u, 4u, failing_entry));

    set_current_service_id(10u);
    const LettuceCapabilityHandle capability =
        lettuce_capability_create(10u, 20u, 3u, LETTUCE_CAP_CALL, 300u);
    const LettuceCapabilityHandle failure_capability =
        lettuce_capability_create(10u, 20u, 4u, LETTUCE_CAP_CALL, 300u);
    assert(capability != LETTUCE_CAPABILITY_INVALID);

    const LettuceCallMessage message = {20u, 3u, 300u, capability};
    assert(lettuce_cross_layer_call(&message) == LETTUCE_STATUS_OK);

    LettuceCallMessage wrong_resource = message;
    wrong_resource.resource_id = 301u;
    assert(lettuce_cross_layer_call(&wrong_resource) == LETTUCE_STATUS_CAPABILITY_DENIED);
    assert(current_service_id() == 10u);
    assert(lettuce_protection_current_domain() == LETTUCE_DOMAIN_ID_INVALID);

    const LettuceCallMessage failure_message = {20u, 4u, 300u, failure_capability};
    assert(lettuce_cross_layer_call(&failure_message) == LETTUCE_STATUS_ERROR);
    assert(current_service_id() == 10u);
    assert(lettuce_protection_current_domain() == LETTUCE_DOMAIN_ID_INVALID);

    LettuceCallMessage wrong_operation = message;
    wrong_operation.operation_id = 4u;
    assert(lettuce_cross_layer_call(&wrong_operation) == LETTUCE_STATUS_CAPABILITY_DENIED);
    const LettuceCallMessage inactive_target = {30u, 3u, 300u, capability};
    assert(lettuce_cross_layer_call(&inactive_target) == LETTUCE_STATUS_INACTIVE_SERVICE);
    LettuceCallMessage revoked = message;
    assert(lettuce_capability_revoke(capability));
    assert(lettuce_cross_layer_call(&revoked) == LETTUCE_STATUS_CAPABILITY_DENIED);
    assert(lettuce_same_layer_call(20u, 3u, 300u, capability) == LETTUCE_STATUS_DIFFERENT_LAYER);

    assert(lettuce_service_registry_unregister(20u));
    assert(lettuce_cross_layer_call(&message) == LETTUCE_STATUS_INVALID_SERVICE);
    return 0;
}
