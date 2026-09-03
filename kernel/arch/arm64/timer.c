/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/arch/arm64/timer.c
 *
 * Purpose:
 *   Implements ARM Generic Timer initialization and scheduler tick support.
 *
 * Flow:
 *   Generic Timer interrupt -> IRQ handling -> scheduler mechanism -> timer
 *   rearm and return.
 */

#include "timer.h"
#include "../../include/arch.h"

static volatile uint64_t g_timer_ticks = 0;
static uint64_t g_timer_interval = 0;
static bool g_timer_enabled = false;

uint64_t lettuce_arch_counter_read(void)
{
	uint64_t counter;
	__asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(counter));
	return counter;
}

uint64_t lettuce_arch_counter_frequency(void)
{
	uint64_t frequency;
	__asm__ __volatile__("mrs %0, cntfrq_el0" : "=r"(frequency));
	return frequency;
}

void lettuce_timer_init(uint32_t hz)
{
	if (hz == 0)
		hz = 100u; /* Default 100 Hz (10 ms period) */

	const uint64_t freq = lettuce_arch_counter_frequency();
	g_timer_interval = freq / (uint64_t)hz;
	g_timer_ticks = 0;
	g_timer_enabled = true;

	/* Program initial countdown value */
	__asm__ __volatile__("msr cntv_tval_el0, %0\n\tisb" : : "r"(g_timer_interval) : "memory");

	/* Enable timer and unmask interrupt: bit 0 = 1 (enable), bit 1 = 0 (unmask) */
	const uint64_t ctl = 1u;
	__asm__ __volatile__("msr cntv_ctl_el0, %0\n\tisb" : : "r"(ctl) : "memory");
}

void lettuce_timer_rearm(void)
{
	if (g_timer_enabled && g_timer_interval > 0)
	{
		__asm__ __volatile__("msr cntv_tval_el0, %0\n\tisb" : : "r"(g_timer_interval) : "memory");
		const uint64_t ctl = 1u;
		__asm__ __volatile__("msr cntv_ctl_el0, %0\n\tisb" : : "r"(ctl) : "memory");
	}
}

void lettuce_timer_disable(void)
{
	g_timer_enabled = false;
	const uint64_t ctl = 0u;
	__asm__ __volatile__("msr cntv_ctl_el0, %0\n\tisb" : : "r"(ctl) : "memory");
}

uint64_t lettuce_timer_get_ticks(void)
{
	return g_timer_ticks;
}

void lettuce_timer_irq_handler(void)
{
	g_timer_ticks++;
	lettuce_timer_rearm();
}
