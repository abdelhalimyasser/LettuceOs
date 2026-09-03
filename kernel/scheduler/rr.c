/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/scheduler/rr.c
 *
 * Purpose:
 *   Implements Round-Robin ready-task selection for the policy interface.
 *
 * Design:
 *   This policy owns rotation state only; the scheduler mechanism performs
 *   lifecycle, context, MMU, and exception handling.
 */

#include "rr.h"

static uint32_t g_rr_last_idx = 0;

static void rr_init(void)
{
	g_rr_last_idx = 0;
}

static void rr_on_tick(LettuceTask *curr, uint64_t tick_delta)
{
	if (curr != 0 && curr->state == LETTUCE_TASK_STATE_RUNNING)
	{
		curr->total_ticks_run += tick_delta;
		if (curr->time_slice_ticks >= tick_delta)
			curr->time_slice_ticks -= tick_delta;
		else
			curr->time_slice_ticks = 0;
	}
}

static LettuceTask *rr_pick_next(LettuceTask *curr)
{
	const uint32_t start_offset = (curr == 0) ? 0 : 1;
	const uint32_t count = LETTUCE_MAX_TASKS;

	/* Circular scan for next READY task */
	for (uint32_t i = start_offset; i < (start_offset + count); ++i)
	{
		uint32_t candidate = (g_rr_last_idx + i) % LETTUCE_MAX_TASKS;
		for (uint16_t gen = 1; gen <= 10; ++gen)
		{
			LettuceTask *t = lettuce_task_get(((uint32_t)gen << 16) | candidate);
			if (t != 0)
			{
				if (t->state == LETTUCE_TASK_STATE_READY)
				{
					g_rr_last_idx = candidate;
					t->time_slice_ticks = t->quantum_ticks;
					return t;
				}
				break;
			}
		}
	}

	/* If no other ready task exists, keep running curr if active */
	if (curr != 0 && curr->state == LETTUCE_TASK_STATE_RUNNING)
	{
		curr->time_slice_ticks = curr->quantum_ticks;
		return curr;
	}

	return 0;
}

static void rr_on_task_ready(LettuceTask *task)
{
	(void)task;
}

static void rr_on_task_sleep(LettuceTask *task)
{
	(void)task;
}

static void rr_on_task_wake(LettuceTask *task)
{
	(void)task;
}

static void rr_on_task_block(LettuceTask *task)
{
	(void)task;
}

static void rr_on_task_exit(LettuceTask *task)
{
	(void)task;
}

const LettuceSchedPolicyOps g_lettuce_sched_rr_ops = {
	.name = "Round-Robin (Baseline)",
	.init = rr_init,
	.on_tick = rr_on_tick,
	.pick_next = rr_pick_next,
	.on_task_ready = rr_on_task_ready,
	.on_task_sleep = rr_on_task_sleep,
	.on_task_wake = rr_on_task_wake,
	.on_task_block = rr_on_task_block,
	.on_task_exit = rr_on_task_exit,
};
