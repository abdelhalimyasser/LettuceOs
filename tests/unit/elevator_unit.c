/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: tests/unit/elevator_unit.c
 *
 * Purpose:
 *   Host-side unit tests for Elevator C policy and mediated target dispatch.
 *
 * Success condition:
 *   Elevator calls require both CALL and CRITICAL permissions before the
 *   target path is entered.
 */

#include <assert.h>

#include "../../include/lettuce/message.h"
#include "../../include/lettuce/service.h"
#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/protection.h"

static LettuceStatus critical_entry(void)
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
    assert(lettuce_dispatch_register(20u, 4u, critical_entry));
    assert(lettuce_dispatch_register(20u, 5u, failing_entry));
    set_current_service_id(10u);

    const LettuceCapabilityHandle normal = lettuce_capability_create(10u, 20u, 4u, LETTUCE_CAP_CALL, 300u);
    const LettuceCapabilityHandle critical = lettuce_capability_create(
        10u, 20u, 4u, LETTUCE_CAP_CALL | LETTUCE_CAP_CRITICAL, 300u);
    const LettuceCapabilityHandle failing = lettuce_capability_create(
        10u, 20u, 5u, LETTUCE_CAP_CALL | LETTUCE_CAP_CRITICAL, 300u);
    const LettuceCallMessage normal_message = {20u, 4u, 300u, normal};
    const LettuceCallMessage critical_message = {20u, 4u, 300u, critical};

    assert(lettuce_elevator_call(&normal_message) == LETTUCE_STATUS_CAPABILITY_DENIED);
    assert(lettuce_elevator_call(&critical_message) == LETTUCE_STATUS_OK);
    assert(lettuce_cross_layer_call(&critical_message) == LETTUCE_STATUS_OK);
    LettuceCallMessage wrong_operation = critical_message;
    wrong_operation.operation_id = 6u;
    assert(lettuce_elevator_call(&wrong_operation) == LETTUCE_STATUS_CAPABILITY_DENIED);
    LettuceCallMessage wrong_resource = critical_message;
    wrong_resource.resource_id = 301u;
    assert(lettuce_elevator_call(&wrong_resource) == LETTUCE_STATUS_CAPABILITY_DENIED);
    assert(lettuce_capability_revoke(critical));
    assert(lettuce_elevator_call(&critical_message) == LETTUCE_STATUS_CAPABILITY_DENIED);
    const LettuceCallMessage failing_message = {20u, 5u, 300u, failing};
    assert(lettuce_elevator_call(&failing_message) == LETTUCE_STATUS_ERROR);
    assert(current_service_id() == 10u);
    assert(lettuce_protection_current_domain() == LETTUCE_DOMAIN_ID_INVALID);
    return 0;
}
