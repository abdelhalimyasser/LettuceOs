/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static volatile uint64_t g_sink = 0u;

static uint64_t direct_call_work(uint64_t value)
{
    value ^= 0x9e3779b97f4a7c15ULL;
    value *= 0x100000001b3ULL;
    g_sink = value;
    return value;
}

int main(void)
{
    const uint64_t iterations = 1000000ULL;
    struct timespec start;
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    uint64_t value = 123u;
    for (uint64_t i = 0; i < iterations; ++i)
    {
        value = direct_call_work(value + i);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    const uint64_t delta_ns = ((uint64_t)end.tv_sec * 1000000000ULL + (uint64_t)end.tv_nsec) -
                             ((uint64_t)start.tv_sec * 1000000000ULL + (uint64_t)start.tv_nsec);

    printf("direct_call total_elapsed_ns=%" PRIu64 " ns_per_call=%" PRIu64 " ops_per_sec=%0.2f\n",
           delta_ns,
           delta_ns / iterations,
           (double)iterations * 1e9 / (double)delta_ns);

    return (value == 0u) ? 1 : 0;
}
