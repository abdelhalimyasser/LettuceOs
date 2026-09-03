/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: benchmarks/direct_call/direct_call_bench.c
 *
 * Purpose:
 *   Measures direct host function work as a baseline for mediated paths.
 *
 * Design:
 *   The benchmark bypasses capability and dispatcher logic intentionally; it
 *   is a host-only baseline, not an isolation or ARM64 timing measurement.
 */

#include "../benchmark_common.h"

static void direct_batch(uint64_t calls, void *context)
{
    (void)context;
    for (uint64_t i = 0; i < calls; ++i)
        lettuce_benchmark_target_work();
}

int main(int argc, char **argv)
{
    LettuceBenchmarkStats stats = lettuce_benchmark_run(direct_batch, NULL);
    if (lettuce_benchmark_has_csv_flag(argc, argv))
        lettuce_benchmark_print_csv("direct_call", stats);
    else
        lettuce_benchmark_print("direct_call", stats);
    return lettuce_benchmark_sink == UINT64_MAX ? 1 : 0;
}
