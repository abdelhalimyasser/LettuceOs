/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: benchmarks/ipc/capability_bench.c
 *
 * Purpose:
 *   Measures host-prototype capability validation independently of dispatch.
 *
 * Design:
 *   The benchmark exercises a configured capability table and reports host
 *   nanosecond timing only.
 */

#include "../benchmark_common.h"
#include "../../include/lettuce/service.h"
#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"

static void capability_batch(uint64_t calls, void *context)
{
    const LettuceCapabilityHandle capability = *(const LettuceCapabilityHandle *)context;
    for (uint64_t i = 0; i < calls; ++i)
        lettuce_benchmark_sink += lettuce_capability_check(capability, 20u, 7u, LETTUCE_CAP_CALL, 300u);
}

int main(int argc, char **argv)
{
    lettuce_capability_init();
    set_current_service_id(10u);
    const LettuceCapabilityHandle capability = lettuce_capability_create(10u, 20u, 7u, LETTUCE_CAP_CALL, 300u);
    if (capability == LETTUCE_CAPABILITY_INVALID)
        return 1;
    LettuceBenchmarkStats stats = lettuce_benchmark_run(capability_batch, (void *)&capability);
    if (lettuce_benchmark_has_csv_flag(argc, argv))
        lettuce_benchmark_print_csv("capability_check", stats);
    else
        lettuce_benchmark_print("capability_check", stats);
    return lettuce_benchmark_sink == UINT64_MAX ? 1 : 0;
}
