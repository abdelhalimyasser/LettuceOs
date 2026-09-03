/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: benchmarks/same_layer/same_layer_bench.c
 *
 * Purpose:
 *   Measures the host-prototype Same-Layer capability-mediated call path.
 *
 * Flow:
 *   Same-classification caller -> validation and target lookup -> target
 *   entry -> host timing summary.
 */

#include "../benchmark_common.h"
#include "../../include/lettuce/capability.h"
#include "../../include/lettuce/service.h"
#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"

static LettuceStatus target_entry(void)
{
    lettuce_benchmark_target_work();
    return LETTUCE_STATUS_OK;
}

static void same_layer_batch(uint64_t calls, void *context)
{
    const LettuceCallMessage *message = context;
    for (uint64_t i = 0; i < calls; ++i)
        if (lettuce_same_layer_call(message->target_service_id, message->operation_id,
                                    message->resource_id, message->capability_handle) != LETTUCE_STATUS_OK)
            abort();
}

int main(int argc, char **argv)
{
    lettuce_capability_init();
    lettuce_service_registry_init();
    const LettuceServiceDescriptor caller = {10u, LETTUCE_LAYER_L3, {0u}, 100u, LETTUCE_SERVICE_FLAG_ACTIVE};
    const LettuceServiceDescriptor target = {20u, LETTUCE_LAYER_L3, {0u}, 200u, LETTUCE_SERVICE_FLAG_ACTIVE};
    if (!lettuce_service_registry_register(caller) || !lettuce_service_registry_register(target) ||
        !lettuce_dispatch_register(20u, 7u, target_entry))
        return 1;
    set_current_service_id(10u);
    const LettuceCapabilityHandle capability = lettuce_capability_create(10u, 20u, 7u, LETTUCE_CAP_CALL, 300u);
    if (capability == LETTUCE_CAPABILITY_INVALID)
        return 2;
    const LettuceCallMessage message = {20u, 7u, 300u, capability};
    LettuceBenchmarkStats stats = lettuce_benchmark_run(same_layer_batch, (void *)&message);
    if (lettuce_benchmark_has_csv_flag(argc, argv))
        lettuce_benchmark_print_csv("same_layer", stats);
    else
        lettuce_benchmark_print("same_layer", stats);
    return lettuce_benchmark_sink == UINT64_MAX ? 1 : 0;
}
