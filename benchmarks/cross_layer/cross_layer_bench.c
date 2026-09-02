/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "../../include/lettuce/message.h"
#include "../../include/lettuce/service.h"
#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"

static LettuceStatus target_entry(void)
{
    return LETTUCE_STATUS_OK;
}

int main(void)
{
    lettuce_capability_init();
    lettuce_service_registry_init();
    lettuce_service_registry_register((LettuceServiceDescriptor){10u, LETTUCE_LAYER_L3, {0u}, 100u, LETTUCE_SERVICE_FLAG_ACTIVE});
    lettuce_service_registry_register((LettuceServiceDescriptor){20u, LETTUCE_LAYER_L1, {0u}, 200u, LETTUCE_SERVICE_FLAG_ACTIVE});
    lettuce_dispatch_register(20u, 7u, target_entry);
    set_current_service_id(10u);
    const LettuceCapabilityHandle capability = lettuce_capability_create(10u, 20u, 7u, LETTUCE_CAP_CALL, 300u);
    const LettuceCallMessage message = {20u, 7u, 300u, capability};
    const uint64_t iterations = 1000000ULL;
    struct timespec start;
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (uint64_t i = 0; i < iterations; ++i)
        if (lettuce_cross_layer_call(&message) != LETTUCE_STATUS_OK)
            return 1;
    clock_gettime(CLOCK_MONOTONIC, &end);
    const uint64_t elapsed = ((uint64_t)end.tv_sec * 1000000000ULL + (uint64_t)end.tv_nsec) -
                             ((uint64_t)start.tv_sec * 1000000000ULL + (uint64_t)start.tv_nsec);
    printf("cross_layer total_elapsed_ns=%" PRIu64 " ns_per_call=%" PRIu64 " ops_per_sec=%0.2f\n",
           elapsed, elapsed / iterations, (double)iterations * 1e9 / (double)elapsed);
    return 0;
}
