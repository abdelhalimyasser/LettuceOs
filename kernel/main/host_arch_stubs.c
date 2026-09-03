/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/main/host_arch_stubs.c
 *
 * Purpose:
 *   Supplies host-prototype implementations of architecture hooks for unit
 *   tests and host benchmarks.
 *
 * This file does not:
 *   Configure EL1 registers, MMU mappings, ASIDs, or ARM64 exception paths.
 */

#ifndef __aarch64__

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "../include/arch.h"
#include "../arch/arm64/irq.h"
#include "../arch/arm64/timer.h"

static uint64_t g_host_timer_ticks = 0;

uint64_t lettuce_arch_counter_read(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

uint64_t lettuce_arch_counter_frequency(void)
{
	return 1000000000ULL;
}

uint64_t lettuce_timer_get_ticks(void)
{
	return g_host_timer_ticks;
}

void lettuce_timer_tick_step(void)
{
	g_host_timer_ticks++;
}

bool lettuce_preempt_is_enabled(void)
{
	return true;
}

void lettuce_preempt_disable(void) {}
void lettuce_preempt_enable(void) {}

uint64_t lettuce_arch_domain_ttbr0_val(LettuceDomainId domain)
{
	(void)domain;
	return 0;
}

#endif /* !__aarch64__ */
