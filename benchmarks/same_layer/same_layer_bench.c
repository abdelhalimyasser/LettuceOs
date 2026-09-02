/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../../include/lettuce/capability.h"
#include "../../include/lettuce/errors.h"
#include "../../include/lettuce/service.h"
#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"

static LettuceStatus bench_handler(void)
{
    return LETTUCE_STATUS_OK;
}

int main(void)
{
    lettuce_capability_init();
    lettuce_service_registry_init();

    const LettuceServiceId caller = 10u;
    const LettuceServiceId target = 20u;
    const LettuceResourceId resource = 300u;
    set_current_service_id(caller);

    LettuceServiceDescriptor caller_desc = {
        .id = caller,
        .layer = LETTUCE_LAYER_L3,
        .domain = 100u,
        .flags = LETTUCE_SERVICE_FLAG_ACTIVE | LETTUCE_SERVICE_FLAG_TRUSTED
    };
    LettuceServiceDescriptor target_desc = {
        .id = target,
        .layer = LETTUCE_LAYER_L3,
        .domain = 200u,
        .flags = LETTUCE_SERVICE_FLAG_ACTIVE | LETTUCE_SERVICE_FLAG_TRUSTED
    };

    if (!lettuce_service_registry_register(caller_desc) || !lettuce_service_registry_register(target_desc))
        return 1;

    if (!lettuce_dispatch_register(target, 7u, bench_handler))
        return 2;

    const LettuceCapabilityHandle cap = lettuce_capability_create(caller, target, 7u, LETTUCE_CAP_CALL, resource);
    if (cap == LETTUCE_CAPABILITY_INVALID)
        return 3;

    const uint64_t iterations = 1000000ULL;
    struct timespec start;
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (uint64_t i = 0; i < iterations; ++i)
    {
        const LettuceStatus status = lettuce_same_layer_call(target, 7u, resource, cap);
        if (status != LETTUCE_STATUS_OK)
            return 4;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    const uint64_t delta_ns = ((uint64_t)end.tv_sec * 1000000000ULL + (uint64_t)end.tv_nsec) -
                             ((uint64_t)start.tv_sec * 1000000000ULL + (uint64_t)start.tv_nsec);

    printf("same_layer total_elapsed_ns=%" PRIu64 " ns_per_call=%" PRIu64 " ops_per_sec=%0.2f\n",
           delta_ns,
           delta_ns / iterations,
           (double)iterations * 1e9 / (double)delta_ns);

    return 0;
}
