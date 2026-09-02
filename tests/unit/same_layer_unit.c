/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <stdint.h>

#include "../../include/lettuce/capability.h"
#include "../../include/lettuce/errors.h"
#include "../../include/lettuce/service.h"
#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/protection.h"

static LettuceStatus ping_handler(void)
{
    return LETTUCE_STATUS_OK;
}

static LettuceStatus submit_frame_handler(void)
{
    return LETTUCE_STATUS_OK;
}

static LettuceStatus failing_handler(void)
{
    return LETTUCE_STATUS_ERROR;
}

int main(void)
{
    lettuce_capability_init();
    lettuce_service_registry_init();

    LettuceServiceDescriptor camera = {
        .id = 10u,
        .layer = LETTUCE_LAYER_L3,
        .domain = 100u,
        .flags = LETTUCE_SERVICE_FLAG_ACTIVE | LETTUCE_SERVICE_FLAG_TRUSTED
    };
    LettuceServiceDescriptor display = {
        .id = 20u,
        .layer = LETTUCE_LAYER_L3,
        .domain = 200u,
        .flags = LETTUCE_SERVICE_FLAG_ACTIVE | LETTUCE_SERVICE_FLAG_TRUSTED
    };

    assert(lettuce_service_registry_register(camera));
    assert(lettuce_service_registry_register(display));
    assert(lettuce_dispatch_register(20u, 1u, ping_handler));
    assert(lettuce_dispatch_register(20u, 2u, submit_frame_handler));
    assert(lettuce_dispatch_register(20u, 3u, failing_handler));

    set_current_service_id(10u);
    const LettuceCapabilityHandle cap = lettuce_capability_create(10u, 20u, 2u, LETTUCE_CAP_CALL, 300u);
    const LettuceCapabilityHandle failure_cap = lettuce_capability_create(10u, 20u, 3u, LETTUCE_CAP_CALL, 300u);
    assert(cap != LETTUCE_CAPABILITY_INVALID);

    assert(lettuce_same_layer_call(20u, 2u, 300u, cap) == LETTUCE_STATUS_OK);
    assert(lettuce_protection_current_domain() == LETTUCE_DOMAIN_ID_INVALID);
    assert(lettuce_same_layer_call(20u, 3u, 300u, failure_cap) == LETTUCE_STATUS_ERROR);
    assert(lettuce_protection_current_domain() == LETTUCE_DOMAIN_ID_INVALID);
    assert(lettuce_same_layer_call(20u, 1u, 300u, cap) == LETTUCE_STATUS_CAPABILITY_DENIED);

    assert(lettuce_same_layer_call(20u, 99u, 300u, cap) == LETTUCE_STATUS_CAPABILITY_DENIED);
    assert(lettuce_same_layer_call(99u, 1u, 300u, cap) == LETTUCE_STATUS_INVALID_SERVICE);
    assert(lettuce_same_layer_call(20u, 1u, 0u, cap) == LETTUCE_STATUS_INVALID_RESOURCE);
    assert(lettuce_same_layer_call(20u, 1u, 301u, cap) == LETTUCE_STATUS_CAPABILITY_DENIED);

    return 0;
}
