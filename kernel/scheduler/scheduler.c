/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/scheduler/scheduler.c
 *
 * Purpose:
 *   Implements scheduler mechanism coordination around pluggable policy
 *   selection and task lifecycle.
 *
 * Flow:
 *   Scheduling event -> policy pick_next -> task/context handoff -> optional
 *   architecture domain entry.
 *
 * Key invariants:
 *   Policy code does not directly own MMU state or exception vectors.
 */

#include "../include/scheduler.h"
#include "../include/task.h"
#include "../include/kernel.h"
#include "../include/arch.h"
#include "policy.h"
#include "rr.h"
#include "eevdf.h"

#ifdef __aarch64__
#include "../arch/arm64/irq.h"
#include "../arch/arm64/timer.h"
#include "../arch/arm64/gic.h"
#else
/* Host stub timer */
static uint64_t g_host_ticks = 0;
static inline uint64_t lettuce_timer_get_ticks(void) { return g_host_ticks; }
#endif

static bool g_scheduler_active = false;
static uint64_t g_switch_count = 0;
static uint64_t g_preempt_count = 0;
static uint64_t g_same_domain_switches = 0;
static uint64_t g_cross_domain_switches = 0;
static uint64_t g_preempt_limit = 0;

static LettuceSchedPolicyType g_current_policy_type = LETTUCE_SCHED_POLICY_RR;
static const LettuceSchedPolicyOps *g_policy_ops = &g_lettuce_sched_rr_ops;

void lettuce_scheduler_init(void)
{
	lettuce_task_init_subsystem();
	g_scheduler_active = false;
	g_switch_count = 0;
	g_preempt_count = 0;
	g_same_domain_switches = 0;
	g_cross_domain_switches = 0;
	g_preempt_limit = 0;

	if (g_policy_ops != 0 && g_policy_ops->init != 0)
		g_policy_ops->init();
}

void lettuce_scheduler_start(void)
{
	g_scheduler_active = true;
}

void lettuce_scheduler_stop(void)
{
	g_scheduler_active = false;
}

void lettuce_scheduler_set_policy(LettuceSchedPolicyType policy)
{
	g_current_policy_type = policy;
	if (policy == LETTUCE_SCHED_POLICY_EEVDF)
		g_policy_ops = &g_lettuce_sched_eevdf_ops;
	else
		g_policy_ops = &g_lettuce_sched_rr_ops;

	if (g_policy_ops != 0 && g_policy_ops->init != 0)
		g_policy_ops->init();
}

LettuceSchedPolicyType lettuce_scheduler_get_policy(void)
{
	return g_current_policy_type;
}

const char *lettuce_scheduler_get_policy_name(void)
{
	return (g_policy_ops != 0) ? g_policy_ops->name : "Unknown";
}

void lettuce_scheduler_set_preempt_limit(uint64_t max_preempts)
{
	g_preempt_limit = max_preempts;
}

bool lettuce_scheduler_is_active(void)
{
	return g_scheduler_active;
}

static void do_schedule(void *trap_frame, bool is_preemption)
{
	if (!g_scheduler_active || trap_frame == 0)
		return;

	LettuceTrapFrame *tf = (LettuceTrapFrame *)trap_frame;
	LettuceTask *curr = lettuce_task_current();

	/* 1. Wake sleeping tasks whose deadlines have elapsed */
	const uint64_t now = lettuce_timer_get_ticks();
	for (uint32_t slot = 0; slot < LETTUCE_MAX_TASKS; ++slot)
	{
		for (uint16_t gen = 1; gen <= 10; ++gen)
		{
			LettuceTask *t = lettuce_task_get(((uint32_t)gen << 16) | slot);
			if (t != 0)
			{
				if (t->state == LETTUCE_TASK_STATE_SLEEPING && now >= t->sleep_deadline_ticks)
				{
					t->state = LETTUCE_TASK_STATE_READY;
					if (g_policy_ops != 0 && g_policy_ops->on_task_wake != 0)
						g_policy_ops->on_task_wake(t);
				}
				break;
			}
		}
	}

	/* 2. Execute policy tick accounting */
	if (curr != 0 && curr->state == LETTUCE_TASK_STATE_RUNNING)
	{
		if (g_policy_ops != 0 && g_policy_ops->on_tick != 0)
			g_policy_ops->on_tick(curr, 1);

		/* If preemption tick and task quantum has not expired, continue */
		if (curr->time_slice_ticks > 0 && is_preemption)
			return;

		/* Quantum expired or yielding: save state and place in ready queue */
		for (uint32_t r = 0; r < 30; ++r)
			curr->context.x[r] = tf->x[r];
		curr->context.x[30] = tf->lr;
		curr->context.elr_el1 = tf->elr;
		curr->context.spsr_el1 = tf->spsr;
		curr->context.sp_el0 = tf->sp_el0;

		curr->state = LETTUCE_TASK_STATE_READY;
		if (g_policy_ops != 0 && g_policy_ops->on_task_ready != 0)
			g_policy_ops->on_task_ready(curr);
	}

	/* 3. Query active scheduling policy for next task */
	LettuceTask *next_task = 0;
	if (g_policy_ops != 0 && g_policy_ops->pick_next != 0)
		next_task = g_policy_ops->pick_next(curr);

	if (next_task == 0)
	{
		if (curr != 0 && curr->state == LETTUCE_TASK_STATE_READY)
		{
			curr->state = LETTUCE_TASK_STATE_RUNNING;
			curr->time_slice_ticks = curr->quantum_ticks;
		}
		return;
	}

	if (next_task == curr)
	{
		/* Current task re-selected: renew quantum and continue running */
		curr->state = LETTUCE_TASK_STATE_RUNNING;
		curr->time_slice_ticks = curr->quantum_ticks;
		return;
	}

	/* 4. Domain isolation handoff */
	if (curr != 0 && next_task->domain_id == curr->domain_id)
	{
		g_same_domain_switches++;
	}
	else
	{
		g_cross_domain_switches++;
#ifdef __aarch64__
		lettuce_mmu_enter(next_task->domain_id);
#endif
	}

	kernel_set_current_service_id(next_task->service_id);

	next_task->state = LETTUCE_TASK_STATE_RUNNING;
	lettuce_task_set_current(next_task);

	/* 5. Restore next_task context into exception trap frame */
	for (uint32_t r = 0; r < 30; ++r)
		tf->x[r] = next_task->context.x[r];
	tf->lr = next_task->context.x[30];
	tf->elr = next_task->context.elr_el1;
	tf->spsr = next_task->context.spsr_el1;
	tf->sp_el0 = next_task->context.sp_el0;

	g_switch_count++;
	if (is_preemption)
	{
		g_preempt_count++;
		if (g_preempt_limit > 0 && g_preempt_count >= g_preempt_limit)
		{
			g_scheduler_active = false;
#ifdef __aarch64__
			lettuce_gic_end_of_interrupt(GIC_INTID_VTIMER);
			lettuce_el0_resume_kernel(LETTUCE_STATUS_OK);
#endif
		}
	}
}

void lettuce_scheduler_tick(void *trap_frame)
{
	do_schedule(trap_frame, true);
}

void lettuce_scheduler_sleep_current(uint64_t ticks, void *trap_frame)
{
	LettuceTask *curr = lettuce_task_current();
	if (curr != 0)
	{
		curr->sleep_deadline_ticks = lettuce_timer_get_ticks() + ticks;
		curr->state = LETTUCE_TASK_STATE_SLEEPING;
		if (g_policy_ops != 0 && g_policy_ops->on_task_sleep != 0)
			g_policy_ops->on_task_sleep(curr);
		do_schedule(trap_frame, false);
	}
}

void lettuce_scheduler_yield(void *trap_frame)
{
	LettuceTask *curr = lettuce_task_current();
	if (curr != 0)
	{
		curr->time_slice_ticks = 0;
		do_schedule(trap_frame, false);
	}
}

uint64_t lettuce_scheduler_switch_count(void)
{
	return g_switch_count;
}

uint64_t lettuce_scheduler_preempt_count(void)
{
	return g_preempt_count;
}

uint64_t lettuce_scheduler_same_domain_switches(void)
{
	return g_same_domain_switches;
}

uint64_t lettuce_scheduler_cross_domain_switches(void)
{
	return g_cross_domain_switches;
}
