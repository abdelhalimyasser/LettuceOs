/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "../../include/lettuce/capability.h"
#include "../../include/lettuce/service.h"
#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"

static void test_valid_capability(void)
{
    lettuce_capability_init();

    const LettuceServiceId owner = 10u;
    const LettuceServiceId target = 20u;
    const LettuceResourceId resource = 30u;
    set_current_service_id(owner);

    const LettuceCapabilityHandle handle = lettuce_capability_create(
        owner,
        target,
        1u,
        LETTUCE_CAP_CALL | LETTUCE_CAP_READ,
        resource);

    assert(handle != LETTUCE_CAPABILITY_INVALID);
    assert(lettuce_capability_check(handle, target, 1u, LETTUCE_CAP_CALL, resource));
    assert(lettuce_capability_check(handle, target, 1u, LETTUCE_CAP_READ, resource));
}

static void test_wrong_owner(void)
{
    lettuce_capability_init();
    set_current_service_id(12u);

    const LettuceCapabilityHandle handle = lettuce_capability_create(
        11u,
        21u,
        1u,
        LETTUCE_CAP_CALL,
        31u);

    assert(handle != LETTUCE_CAPABILITY_INVALID);
    assert(!lettuce_capability_check(handle, 21u, 1u, LETTUCE_CAP_CALL, 31u));
}

static void test_wrong_target(void)
{
    lettuce_capability_init();
    set_current_service_id(11u);

    const LettuceCapabilityHandle handle = lettuce_capability_create(
        11u,
        21u,
        1u,
        LETTUCE_CAP_CALL,
        31u);

    assert(handle != LETTUCE_CAPABILITY_INVALID);
    assert(!lettuce_capability_check(handle, 22u, 1u, LETTUCE_CAP_CALL, 31u));
}

static void test_wrong_operation(void)
{
    lettuce_capability_init();
    set_current_service_id(11u);

    const LettuceCapabilityHandle handle = lettuce_capability_create(
        11u,
        21u,
        1u,
        LETTUCE_CAP_CALL,
        31u);

    assert(handle != LETTUCE_CAPABILITY_INVALID);
    assert(!lettuce_capability_check(handle, 21u, 1u, LETTUCE_CAP_READ, 31u));
}

static void test_wrong_resource(void)
{
    lettuce_capability_init();
    set_current_service_id(11u);

    const LettuceCapabilityHandle handle = lettuce_capability_create(
        11u,
        21u,
        1u,
        LETTUCE_CAP_CALL,
        31u);

    assert(handle != LETTUCE_CAPABILITY_INVALID);
    assert(!lettuce_capability_check(handle, 21u, 1u, LETTUCE_CAP_CALL, 32u));
}

static void test_revoked_handle(void)
{
    lettuce_capability_init();
    set_current_service_id(11u);

    const LettuceCapabilityHandle handle = lettuce_capability_create(
        11u,
        21u,
        1u,
        LETTUCE_CAP_CALL,
        31u);

    assert(handle != LETTUCE_CAPABILITY_INVALID);
    assert(lettuce_capability_revoke(handle));
    assert(!lettuce_capability_check(handle, 21u, 1u, LETTUCE_CAP_CALL, 31u));
}

static void test_stale_handle(void)
{
    lettuce_capability_init();
    set_current_service_id(11u);

    const LettuceCapabilityHandle handle = lettuce_capability_create(
        11u,
        21u,
        1u,
        LETTUCE_CAP_CALL,
        31u);

    assert(handle != LETTUCE_CAPABILITY_INVALID);
    assert(lettuce_capability_revoke(handle));

    const uint32_t slot = (handle & 0xFFFFu) - 1u;
    const uint32_t generation = (uint32_t)(handle >> 16u);
    set_current_service_id(12u);
    const LettuceCapabilityHandle replacement = lettuce_capability_create(
        12u,
        22u,
        1u,
        LETTUCE_CAP_CALL,
        32u);
    assert(replacement != LETTUCE_CAPABILITY_INVALID);
    (void)slot;
    (void)generation;

    assert(!lettuce_capability_check(handle, 21u, 1u, LETTUCE_CAP_CALL, 31u));
    assert(lettuce_capability_check(replacement, 22u, 1u, LETTUCE_CAP_CALL, 32u));
}

static void test_malformed_handle(void)
{
    lettuce_capability_init();

    assert(!lettuce_capability_check(LETTUCE_CAPABILITY_INVALID, 2u, 1u, LETTUCE_CAP_CALL, 3u));
    assert(!lettuce_capability_check(0x00010000u, 2u, 1u, LETTUCE_CAP_CALL, 3u));
    assert(!lettuce_capability_check(0xFFFFFFFFu, 2u, 1u, LETTUCE_CAP_CALL, 3u));
}

static void test_valid_registration(void)
{
    lettuce_service_registry_init();

    LettuceServiceDescriptor descriptor = {
        .id = 10u,
        .layer = LETTUCE_LAYER_L2,
        .flags = LETTUCE_SERVICE_FLAG_ACTIVE | LETTUCE_SERVICE_FLAG_TRUSTED
    };

    assert(lettuce_service_registry_register(descriptor));
    assert(lettuce_service_registry_is_active(10u));

    const LettuceServiceDescriptor *lookup = lettuce_service_registry_lookup(10u);
    assert(lookup != NULL);
    assert(lookup->id == 10u);
    assert(lookup->layer == LETTUCE_LAYER_L2);
}

static void test_duplicate_registration(void)
{
    lettuce_service_registry_init();

    LettuceServiceDescriptor descriptor = {
        .id = 20u,
        .layer = LETTUCE_LAYER_L1,
        .flags = LETTUCE_SERVICE_FLAG_ACTIVE
    };

    assert(lettuce_service_registry_register(descriptor));
    assert(!lettuce_service_registry_register(descriptor));
}

static void test_invalid_service(void)
{
    lettuce_service_registry_init();

    LettuceServiceDescriptor invalid = {
        .id = LETTUCE_SERVICE_ID_INVALID,
        .layer = LETTUCE_LAYER_MAIN,
        .flags = LETTUCE_SERVICE_FLAG_ACTIVE
    };

    assert(!lettuce_service_registry_register(invalid));
    assert(!lettuce_service_registry_lookup(LETTUCE_SERVICE_ID_INVALID));
}

static void test_inactive_service(void)
{
    lettuce_service_registry_init();

    LettuceServiceDescriptor descriptor = {
        .id = 30u,
        .layer = LETTUCE_LAYER_L3,
        .flags = LETTUCE_SERVICE_FLAG_ACTIVE
    };

    assert(lettuce_service_registry_register(descriptor));
    assert(lettuce_service_registry_unregister(30u));
    assert(!lettuce_service_registry_is_active(30u));
    assert(lettuce_service_registry_lookup(30u) == NULL);
}

static void test_current_service_lookup(void)
{
    lettuce_service_registry_init();
    set_current_service_id(40u);

    assert(current_service_id() == 40u);

    LettuceServiceDescriptor descriptor = {
        .id = 40u,
        .layer = LETTUCE_LAYER_L4,
        .flags = LETTUCE_SERVICE_FLAG_ACTIVE | LETTUCE_SERVICE_FLAG_RESTARTABLE
    };

    assert(lettuce_service_registry_register(descriptor));
    assert(lettuce_service_registry_lookup(current_service_id()) != NULL);
    assert(lettuce_service_registry_validate(current_service_id()));
}

static void test_capability_uses_trusted_current_identity(void)
{
    lettuce_capability_init();
    lettuce_service_registry_init();

    set_current_service_id(50u);

    const LettuceCapabilityHandle handle = lettuce_capability_create(
        50u,
        60u,
        1u,
        LETTUCE_CAP_CALL,
        70u);

    assert(handle != LETTUCE_CAPABILITY_INVALID);
    assert(lettuce_capability_check(handle, 60u, 1u, LETTUCE_CAP_CALL, 70u));
}

static void test_constant_time_slot_reuse(void)
{
    static LettuceCapabilityHandle handles[LETTUCE_CAPABILITY_TABLE_SIZE];

    lettuce_capability_init();
    set_current_service_id(80u);
    for (uint32_t i = 0; i < LETTUCE_CAPABILITY_TABLE_SIZE; ++i)
    {
        handles[i] = lettuce_capability_create(80u, 81u, 1u, LETTUCE_CAP_CALL, 82u);
        assert(handles[i] != LETTUCE_CAPABILITY_INVALID);
    }
    assert(lettuce_capability_create(80u, 81u, 1u, LETTUCE_CAP_CALL, 82u) == LETTUCE_CAPABILITY_INVALID);
    assert(lettuce_capability_revoke(handles[123u]));
    assert(!lettuce_capability_revoke(handles[123u]));
    const LettuceCapabilityHandle replacement =
        lettuce_capability_create(80u, 81u, 1u, LETTUCE_CAP_CALL, 82u);
    assert(replacement != LETTUCE_CAPABILITY_INVALID);
    assert(replacement != handles[123u]);
}

int main(void)
{
    test_valid_capability();
    test_wrong_owner();
    test_wrong_target();
    test_wrong_operation();
    test_wrong_resource();
    test_revoked_handle();
    test_stale_handle();
    test_malformed_handle();
    test_valid_registration();
    test_duplicate_registration();
    test_invalid_service();
    test_inactive_service();
    test_current_service_lookup();
    test_capability_uses_trusted_current_identity();
    test_constant_time_slot_reuse();
    return 0;
}
