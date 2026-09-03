/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/main/task.c
 *
 * Purpose:
 *   Implements the bounded task registry and task lifecycle state.
 *
 * Responsibilities:
 *   - Create, resolve, and release task records.
 *   - Preserve task identifiers and scheduler-visible state without heap use.
 */

#include "../include/task.h"

static LettuceTask g_task_table[LETTUCE_MAX_TASKS];
static LettuceTask *g_current_task = 0;

void lettuce_task_init_subsystem(void)
{
	for (uint32_t i = 0; i < LETTUCE_MAX_TASKS; ++i)
	{
		g_task_table[i].id = 0;
		g_task_table[i].generation = 1;
		g_task_table[i].service_id = 0;
		g_task_table[i].domain_id = 0;
		g_task_table[i].priority = 0;
		g_task_table[i].padding1 = 0;
		g_task_table[i].state = LETTUCE_TASK_STATE_UNUSED;
		g_task_table[i].weight = LETTUCE_EEVDF_BASE_WEIGHT;
		g_task_table[i].requested_slice_ticks = LETTUCE_EEVDF_DEFAULT_SLICE;
		g_task_table[i].padding2 = 0;
		g_task_table[i].time_slice_ticks = 0;
		g_task_table[i].quantum_ticks = LETTUCE_EEVDF_DEFAULT_SLICE;
		g_task_table[i].vruntime = 0;
		g_task_table[i].vdeadline = 0;
		g_task_table[i].sleep_deadline_ticks = 0;
		g_task_table[i].total_ticks_run = 0;
		g_task_table[i].stack_base = 0;
		g_task_table[i].stack_size = 0;
		g_task_table[i].name = 0;
		for (uint32_t r = 0; r < 31; ++r)
			g_task_table[i].context.x[r] = 0;
		g_task_table[i].context.sp_el0 = 0;
		g_task_table[i].context.elr_el1 = 0;
		g_task_table[i].context.spsr_el1 = 0;
	}
	g_current_task = 0;
}

LettuceTask *lettuce_task_create(
	LettuceServiceId service_id,
	LettuceDomainId domain_id,
	uintptr_t entry_pc,
	uintptr_t stack_top,
	const char *name)
{
	for (uint32_t i = 0; i < LETTUCE_MAX_TASKS; ++i)
	{
		if (g_task_table[i].state == LETTUCE_TASK_STATE_UNUSED ||
		    g_task_table[i].state == LETTUCE_TASK_STATE_TERMINATED)
		{
			LettuceTask *t = &g_task_table[i];
			const uint16_t gen = (t->generation == 0) ? 1 : t->generation;
			t->generation = gen;
			t->id = ((uint32_t)gen << 16) | (uint32_t)i;
			t->service_id = service_id;
			t->domain_id = domain_id;
			t->priority = 0;
			t->padding1 = 0;
			t->state = LETTUCE_TASK_STATE_READY;
			t->weight = LETTUCE_EEVDF_BASE_WEIGHT;
			t->requested_slice_ticks = LETTUCE_EEVDF_DEFAULT_SLICE;
			t->padding2 = 0;
			t->quantum_ticks = LETTUCE_EEVDF_DEFAULT_SLICE;
			t->time_slice_ticks = LETTUCE_EEVDF_DEFAULT_SLICE;
			t->vruntime = 0;
			t->vdeadline = 0;
			t->sleep_deadline_ticks = 0;
			t->total_ticks_run = 0;
			t->name = name;

			for (uint32_t r = 0; r < 31; ++r)
				t->context.x[r] = 0;
			t->context.sp_el0 = stack_top;
			t->context.elr_el1 = entry_pc;
			t->context.spsr_el1 = 0; /* EL0t with interrupts unmasked */

			return t;
		}
	}
	return 0;
}

LettuceTask *lettuce_task_get(LettuceTaskId id)
{
	const uint32_t slot = id & 0xffffu;
	const uint16_t gen = (uint16_t)(id >> 16u);
	if (slot >= LETTUCE_MAX_TASKS)
		return 0;
	if (g_task_table[slot].generation != gen)
		return 0;
	if (g_task_table[slot].state == LETTUCE_TASK_STATE_UNUSED)
		return 0;
	return &g_task_table[slot];
}

LettuceTask *lettuce_task_current(void)
{
	return g_current_task;
}

void lettuce_task_set_current(LettuceTask *task)
{
	g_current_task = task;
}

void lettuce_task_terminate(LettuceTaskId id)
{
	LettuceTask *t = lettuce_task_get(id);
	if (t != 0)
	{
		t->state = LETTUCE_TASK_STATE_TERMINATED;
		t->generation++;
		if (t->generation == 0)
			t->generation = 1;
		if (g_current_task == t)
			g_current_task = 0;
	}
}

uint32_t lettuce_task_active_count(void)
{
	uint32_t count = 0;
	for (uint32_t i = 0; i < LETTUCE_MAX_TASKS; ++i)
	{
		if (g_task_table[i].state != LETTUCE_TASK_STATE_UNUSED &&
		    g_task_table[i].state != LETTUCE_TASK_STATE_TERMINATED)
		{
			count++;
		}
	}
	return count;
}

void lettuce_task_set_weight(LettuceTask *task, uint32_t weight)
{
	if (task != 0)
	{
		/* Enforce non-zero weight (minimum 1, maximum 10240) */
		if (weight == 0)
			weight = 1;
		else if (weight > 10240u)
			weight = 10240u;
		task->weight = weight;
	}
}

uint32_t lettuce_task_get_weight(const LettuceTask *task)
{
	return (task != 0) ? task->weight : LETTUCE_EEVDF_BASE_WEIGHT;
}

void lettuce_task_set_slice(LettuceTask *task, uint32_t slice_ticks)
{
	if (task != 0)
	{
		if (slice_ticks == 0)
			slice_ticks = 1;
		task->requested_slice_ticks = slice_ticks;
		task->quantum_ticks = slice_ticks;
	}
}

uint32_t lettuce_task_get_slice(const LettuceTask *task)
{
	return (task != 0) ? task->requested_slice_ticks : LETTUCE_EEVDF_DEFAULT_SLICE;
}
