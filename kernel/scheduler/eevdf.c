/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/scheduler/eevdf.c
 *
 * Purpose:
 *   Implements EEVDF policy accounting and ready-task selection.
 *
 * Design:
 *   This policy selects eligible tasks by virtual-deadline state; scheduler
 *   mechanism code owns context, MMU, IRQ, and exception-return mechanics.
 */

#include "eevdf.h"

#define LETTUCE_EEVDF_SCALE 65536ULL

/*
 * System Virtual Time (V_sys).
 * Tracks monotonic virtual progression across all active tasks.
 */
static uint64_t g_eevdf_vsys = 0;

static uint64_t calc_vslice(const LettuceTask *task)
{
	const uint32_t weight = (task != 0 && task->weight > 0) ? task->weight : LETTUCE_EEVDF_BASE_WEIGHT;
	const uint64_t slice = (task != 0 && task->requested_slice_ticks > 0) ? task->requested_slice_ticks : LETTUCE_EEVDF_DEFAULT_SLICE;
	return (slice * LETTUCE_EEVDF_SCALE) / (uint64_t)weight;
}

uint64_t lettuce_eevdf_calc_deadline(const LettuceTask *task)
{
	if (task == 0)
		return 0;
	return task->vruntime + calc_vslice(task);
}

uint64_t lettuce_eevdf_get_vsys(void)
{
	return g_eevdf_vsys;
}

bool lettuce_eevdf_is_eligible(const LettuceTask *task)
{
	if (task == 0 || task->state != LETTUCE_TASK_STATE_READY)
		return false;
	return task->vruntime <= g_eevdf_vsys;
}

int64_t lettuce_eevdf_get_lag(const LettuceTask *task)
{
	if (task == 0)
		return 0;
	/* Lag = V_sys - V_i. Positive means task is entitled to run */
	return (int64_t)g_eevdf_vsys - (int64_t)task->vruntime;
}

static uint32_t get_active_weight_sum(void)
{
	uint32_t total_weight = 0;
	for (uint32_t slot = 0; slot < LETTUCE_MAX_TASKS; ++slot)
	{
		for (uint16_t gen = 1; gen <= 10; ++gen)
		{
			LettuceTask *t = lettuce_task_get(((uint32_t)gen << 16) | slot);
			if (t != 0)
			{
				if (t->state == LETTUCE_TASK_STATE_READY || t->state == LETTUCE_TASK_STATE_RUNNING)
				{
					total_weight += (t->weight > 0) ? t->weight : LETTUCE_EEVDF_BASE_WEIGHT;
				}
				break;
			}
		}
	}
	return (total_weight > 0) ? total_weight : LETTUCE_EEVDF_BASE_WEIGHT;
}

static void eevdf_init(void)
{
	g_eevdf_vsys = 0;
}

static void eevdf_on_tick(LettuceTask *curr, uint64_t tick_delta)
{
	if (curr != 0 && curr->state == LETTUCE_TASK_STATE_RUNNING)
	{
		curr->total_ticks_run += tick_delta;

		/* Advance current task virtual runtime: delta_v = (ticks * SCALE) / w_i */
		const uint32_t weight = (curr->weight > 0) ? curr->weight : LETTUCE_EEVDF_BASE_WEIGHT;
		const uint64_t delta_v = (tick_delta * LETTUCE_EEVDF_SCALE) / (uint64_t)weight;
		curr->vruntime += delta_v;

		/* Advance system virtual time: delta_vsys = (ticks * SCALE) / W_active */
		const uint32_t active_weight = get_active_weight_sum();
		const uint64_t delta_vsys = (tick_delta * LETTUCE_EEVDF_SCALE) / (uint64_t)active_weight;
		g_eevdf_vsys += delta_vsys;

		/* Decrement remaining slice */
		if (curr->time_slice_ticks >= tick_delta)
			curr->time_slice_ticks -= tick_delta;
		else
			curr->time_slice_ticks = 0;
	}
}

static LettuceTask *eevdf_pick_next(LettuceTask *curr)
{
	LettuceTask *best_eligible = 0;
	uint64_t earliest_deadline = UINT64_MAX;

	LettuceTask *min_vruntime_task = 0;
	uint64_t min_vruntime = UINT64_MAX;

	uint32_t ready_count = 0;

	/* 1. Bounded scan of task table for READY candidates */
	for (uint32_t slot = 0; slot < LETTUCE_MAX_TASKS; ++slot)
	{
		for (uint16_t gen = 1; gen <= 10; ++gen)
		{
			LettuceTask *t = lettuce_task_get(((uint32_t)gen << 16) | slot);
			if (t != 0)
			{
				if (t->state == LETTUCE_TASK_STATE_READY)
				{
					ready_count++;

					/* Track minimum vruntime for fallback if none are eligible */
					if (t->vruntime < min_vruntime)
					{
						min_vruntime = t->vruntime;
						min_vruntime_task = t;
					}

					/* Check EEVDF Eligibility: V_i <= V_sys */
					if (t->vruntime <= g_eevdf_vsys)
					{
						/* Calculate deadline if not yet computed */
						if (t->vdeadline < t->vruntime)
							t->vdeadline = lettuce_eevdf_calc_deadline(t);

						/* Pick earliest eligible virtual deadline */
						if (t->vdeadline < earliest_deadline)
						{
							earliest_deadline = t->vdeadline;
							best_eligible = t;
						}
					}
				}
				break;
			}
		}
	}

	/* 2. If eligible ready tasks exist, pick earliest deadline */
	if (best_eligible != 0)
	{
		best_eligible->time_slice_ticks = best_eligible->requested_slice_ticks;
		best_eligible->vdeadline = lettuce_eevdf_calc_deadline(best_eligible);
		return best_eligible;
	}

	/* 3. If tasks are ready but none are eligible, advance V_sys to catch up */
	if (ready_count > 0 && min_vruntime_task != 0)
	{
		g_eevdf_vsys = min_vruntime;
		min_vruntime_task->time_slice_ticks = min_vruntime_task->requested_slice_ticks;
		min_vruntime_task->vdeadline = lettuce_eevdf_calc_deadline(min_vruntime_task);
		return min_vruntime_task;
	}

	/* 4. No other ready task: preserve curr if still RUNNING */
	if (curr != 0 && curr->state == LETTUCE_TASK_STATE_RUNNING)
	{
		curr->time_slice_ticks = curr->requested_slice_ticks;
		curr->vdeadline = lettuce_eevdf_calc_deadline(curr);
		return curr;
	}

	return 0;
}

static void eevdf_on_task_ready(LettuceTask *task)
{
	if (task != 0)
	{
		/* If task is new or behind system time, align to avoid excessive lag accumulation */
		if (task->vruntime < g_eevdf_vsys)
			task->vruntime = g_eevdf_vsys;
		task->vdeadline = lettuce_eevdf_calc_deadline(task);
	}
}

static void eevdf_on_task_sleep(LettuceTask *task)
{
	(void)task;
}

static void eevdf_on_task_wake(LettuceTask *task)
{
	if (task != 0)
	{
		/*
		 * EEVDF Wakeup Invariant:
		 * Align virtual runtime with V_sys upon wakeup to prevent returning tasks
		 * from accumulating unbounded lag credit while dormant.
		 */
		if (task->vruntime < g_eevdf_vsys)
			task->vruntime = g_eevdf_vsys;
		task->vdeadline = lettuce_eevdf_calc_deadline(task);
	}
}

static void eevdf_on_task_block(LettuceTask *task)
{
	(void)task;
}

static void eevdf_on_task_exit(LettuceTask *task)
{
	(void)task;
}

const LettuceSchedPolicyOps g_lettuce_sched_eevdf_ops = {
	.name = "EEVDF (Earliest Eligible Virtual Deadline First)",
	.init = eevdf_init,
	.on_tick = eevdf_on_tick,
	.pick_next = eevdf_pick_next,
	.on_task_ready = eevdf_on_task_ready,
	.on_task_sleep = eevdf_on_task_sleep,
	.on_task_wake = eevdf_on_task_wake,
	.on_task_block = eevdf_on_task_block,
	.on_task_exit = eevdf_on_task_exit,
};
