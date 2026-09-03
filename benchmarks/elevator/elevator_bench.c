/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: benchmarks/elevator/elevator_bench.c
 *
 * Purpose:
 *   Measures the host-prototype Elevator policy path after capability setup.
 *
 * Flow:
 *   Caller context -> CALL and CRITICAL capability checks -> registered
 *   target entry -> host timing summary.
 */

#include "../benchmark_common.h"
#include "../../include/lettuce/message.h"
#include "../../include/lettuce/service.h"
#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"

static LettuceStatus target_entry(void)
{
    lettuce_benchmark_target_work();
    return LETTUCE_STATUS_OK;
}

static void elevator_batch(uint64_t calls, void *context)
{
    const LettuceCallMessage *message = context;
    for (uint64_t i = 0; i < calls; ++i)
        if (lettuce_elevator_call(message) != LETTUCE_STATUS_OK)
            abort();
}

int main(int argc, char **argv)
{
    lettuce_capability_init();
    lettuce_service_registry_init();
    if (!lettuce_service_registry_register((LettuceServiceDescriptor){10u, LETTUCE_LAYER_L3, {0u}, 100u, LETTUCE_SERVICE_FLAG_ACTIVE}) ||
        !lettuce_service_registry_register((LettuceServiceDescriptor){20u, LETTUCE_LAYER_L1, {0u}, 200u, LETTUCE_SERVICE_FLAG_ACTIVE}) ||
        !lettuce_dispatch_register(20u, 7u, target_entry))
        return 1;
    set_current_service_id(10u);
    const LettuceCapabilityHandle capability = lettuce_capability_create(
        10u, 20u, 7u, LETTUCE_CAP_CALL | LETTUCE_CAP_CRITICAL, 300u);
    if (capability == LETTUCE_CAPABILITY_INVALID)
        return 2;
    const LettuceCallMessage message = {20u, 7u, 300u, capability};
    LettuceBenchmarkStats stats = lettuce_benchmark_run(elevator_batch, (void *)&message);
    if (lettuce_benchmark_has_csv_flag(argc, argv))
        lettuce_benchmark_print_csv("elevator", stats);
    else
        lettuce_benchmark_print("elevator", stats);
    return lettuce_benchmark_sink == UINT64_MAX ? 1 : 0;
}
