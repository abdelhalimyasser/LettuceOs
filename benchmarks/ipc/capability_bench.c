/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "../../include/lettuce/service.h"
#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"

int main(void)
{
    lettuce_capability_init();
    set_current_service_id(10u);
    const LettuceCapabilityHandle capability = lettuce_capability_create(10u, 20u, 7u, LETTUCE_CAP_CALL, 300u);
    const uint64_t iterations = 1000000ULL;
    volatile uint64_t successes = 0u;
    struct timespec start;
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (uint64_t i = 0; i < iterations; ++i)
        successes += lettuce_capability_check(capability, 20u, 7u, LETTUCE_CAP_CALL, 300u);
    clock_gettime(CLOCK_MONOTONIC, &end);
    const uint64_t elapsed = ((uint64_t)end.tv_sec * 1000000000ULL + (uint64_t)end.tv_nsec) -
                             ((uint64_t)start.tv_sec * 1000000000ULL + (uint64_t)start.tv_nsec);
    printf("capability_check total_elapsed_ns=%" PRIu64 " ns_per_check=%" PRIu64 " ops_per_sec=%0.2f successes=%" PRIu64 "\n",
           elapsed, elapsed / iterations, (double)iterations * 1e9 / (double)elapsed, successes);
    return successes == iterations ? 0 : 1;
}
