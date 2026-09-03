/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: tests/unit/task_scheduler_unit.c
 *
 * Purpose:
 *   Host-side unit tests for bounded task lifecycle and scheduler mechanism
 *   coordination.
 *
 * Success condition:
 *   Task state transitions and policy-driven selection retain the scheduler
 *   mechanism invariants.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "task.h"
#include "scheduler.h"
#include "arch.h"

int main(void)
{
	printf("Running task_scheduler_unit (Round-Robin Baseline) tests...\n");

	lettuce_scheduler_init();
	assert(!lettuce_scheduler_is_active());
	assert(lettuce_scheduler_get_policy() == LETTUCE_SCHED_POLICY_RR);
	printf("  [✓] Default policy initialized as: %s\n", lettuce_scheduler_get_policy_name());

	/* 1. Create tasks across domains */
	LettuceTask *t1 = lettuce_task_create(10, 100, 0x1000, 0x2000, "Task1");
	assert(t1 != NULL);
	assert(t1->state == LETTUCE_TASK_STATE_READY);
	assert(t1->service_id == 10);
	assert(t1->domain_id == 100);

	LettuceTask *t2 = lettuce_task_create(20, 200, 0x3000, 0x4000, "Task2");
	assert(t2 != NULL);
	assert(t2->domain_id == 200);

	LettuceTask *t3 = lettuce_task_create(30, 100, 0x5000, 0x6000, "Task3");
	assert(t3 != NULL);
	assert(t3->domain_id == 100); /* Same domain as t1 */

	assert(lettuce_task_active_count() == 3);

	/* 2. Generation & Stale TaskId checks */
	const LettuceTaskId old_t3_id = t3->id;
	lettuce_task_terminate(old_t3_id);
	assert(t3->state == LETTUCE_TASK_STATE_TERMINATED);
	assert(lettuce_task_get(old_t3_id) == NULL); /* Stale handle lookup returns NULL */
	assert(lettuce_task_active_count() == 2);

	/* Re-create Task3 */
	LettuceTask *t3_new = lettuce_task_create(30, 100, 0x5000, 0x6000, "Task3_New");
	assert(t3_new != NULL);
	assert(t3_new->id != old_t3_id);
	assert(lettuce_task_active_count() == 3);

	/* 3. Preemption & Deterministic Rotation */
	lettuce_scheduler_start();
	assert(lettuce_scheduler_is_active());

	LettuceTrapFrame dummy_frame;
	memset(&dummy_frame, 0, sizeof(dummy_frame));

	/* Tick 1: Schedule first task (t1) */
	lettuce_scheduler_tick(&dummy_frame);
	assert(lettuce_task_current() == t1);
	assert(t1->state == LETTUCE_TASK_STATE_RUNNING);

	/* Exhaust quantum */
	t1->time_slice_ticks = 0;
	lettuce_scheduler_tick(&dummy_frame);
	assert(lettuce_task_current() == t2);
	assert(t2->state == LETTUCE_TASK_STATE_RUNNING);
	assert(t1->state == LETTUCE_TASK_STATE_READY);

	/* Exhaust quantum for t2 -> switches to t3_new */
	t2->time_slice_ticks = 0;
	lettuce_scheduler_tick(&dummy_frame);
	assert(lettuce_task_current() == t3_new);

	/* Exhaust quantum for t3_new -> cycles back to t1 */
	t3_new->time_slice_ticks = 0;
	lettuce_scheduler_tick(&dummy_frame);
	assert(lettuce_task_current() == t1);
	printf("  [✓] Deterministic Round-Robin rotation verified (t1 -> t2 -> t3 -> t1)\n");

	/* 4. Sleeping tasks are skipped */
	t2->state = LETTUCE_TASK_STATE_SLEEPING;
	t2->sleep_deadline_ticks = 999999; /* Future deadline */

	t1->time_slice_ticks = 0;
	lettuce_scheduler_tick(&dummy_frame);
	/* Should skip t2 directly to t3_new */
	assert(lettuce_task_current() == t3_new);
	printf("  [✓] Sleeping task (t2) correctly skipped\n");

	/* 5. Terminated tasks are skipped */
	lettuce_task_terminate(t3_new->id);
	t1->time_slice_ticks = 0;
	lettuce_scheduler_tick(&dummy_frame);
	/* Since t2 is sleeping and t3 is terminated, t1 runs */
	assert(lettuce_task_current() == t1);
	printf("  [✓] Terminated task (t3) correctly skipped\n");

	/* 6. Same-domain vs cross-domain switch tracking */
	assert(lettuce_scheduler_switch_count() > 0);
	assert(lettuce_scheduler_cross_domain_switches() > 0);
	printf("  [✓] Domain switch metrics: %lu cross-domain, %lu same-domain\n",
	       (unsigned long)lettuce_scheduler_cross_domain_switches(),
	       (unsigned long)lettuce_scheduler_same_domain_switches());

	lettuce_scheduler_stop();
	assert(!lettuce_scheduler_is_active());

	printf("[PASS] task_scheduler_unit (Round-Robin) passed cleanly!\n");
	return 0;
}
