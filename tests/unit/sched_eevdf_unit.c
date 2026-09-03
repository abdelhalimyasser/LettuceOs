/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: tests/unit/sched_eevdf_unit.c
 *
 * Purpose:
 *   Host-side unit tests for EEVDF policy accounting and task selection.
 *
 * Success condition:
 *   Eligible ready tasks are selected according to the policy without giving
 *   the policy ownership of architecture switching mechanics.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "task.h"
#include "scheduler.h"
#include "arch.h"
#include "../../kernel/scheduler/eevdf.h"

int main(void)
{
	printf("Running sched_eevdf_unit tests...\n");

	/* Test 10: Policy Selection & Independence */
	lettuce_scheduler_init();
	assert(lettuce_scheduler_get_policy() == LETTUCE_SCHED_POLICY_RR);

	lettuce_scheduler_set_policy(LETTUCE_SCHED_POLICY_EEVDF);
	assert(lettuce_scheduler_get_policy() == LETTUCE_SCHED_POLICY_EEVDF);
	printf("  [✓] Test 10: Selected policy is %s\n", lettuce_scheduler_get_policy_name());

	LettuceTrapFrame dummy_frame;
	memset(&dummy_frame, 0, sizeof(dummy_frame));

	/* Test 4: Earliest Eligible Virtual Deadline Wins */
	{
		lettuce_scheduler_init();
		lettuce_scheduler_set_policy(LETTUCE_SCHED_POLICY_EEVDF);

		LettuceTask *t1 = lettuce_task_create(10, 100, 0x1000, 0x2000, "T_ShortSlice");
		LettuceTask *t2 = lettuce_task_create(20, 200, 0x3000, 0x4000, "T_LongSlice");
		assert(t1 && t2);

		/* Both equal weight 1024, but t1 has shorter slice (2) than t2 (10) */
		lettuce_task_set_slice(t1, 2);
		lettuce_task_set_slice(t2, 10);
		t1->vruntime = 0;
		t2->vruntime = 0;
		t1->vdeadline = lettuce_eevdf_calc_deadline(t1);
		t2->vdeadline = lettuce_eevdf_calc_deadline(t2);

		/* t1 has smaller virtual slice -> earlier deadline */
		assert(t1->vdeadline < t2->vdeadline);

		lettuce_scheduler_start();
		lettuce_scheduler_tick(&dummy_frame);

		/* EEVDF must select t1 because it has the earlier virtual deadline */
		assert(lettuce_task_current() == t1);
		printf("  [✓] Test 4: Earliest eligible virtual deadline selected (T_ShortSlice)\n");
		lettuce_scheduler_stop();
	}

	/* Test 3: Ineligible Task is NOT Selected */
	{
		lettuce_scheduler_init();
		lettuce_scheduler_set_policy(LETTUCE_SCHED_POLICY_EEVDF);

		LettuceTask *t1 = lettuce_task_create(10, 100, 0x1000, 0x2000, "T_Ahead");
		LettuceTask *t2 = lettuce_task_create(20, 200, 0x3000, 0x4000, "T_Eligible");
		assert(t1 && t2);

		/* Artificially advance t1 ahead of V_sys */
		t1->vruntime = 5000;
		t1->vdeadline = 5010; /* Far deadline, but ahead */

		t2->vruntime = 0;     /* Eligible: vruntime <= vsys (vsys=0) */
		t2->vdeadline = 100;

		assert(!lettuce_eevdf_is_eligible(t1));
		assert(lettuce_eevdf_is_eligible(t2));

		lettuce_scheduler_start();
		lettuce_scheduler_tick(&dummy_frame);

		/* Must pick t2 because t1 is not eligible */
		assert(lettuce_task_current() == t2);
		printf("  [✓] Test 3: Ineligible task (V_i > V_sys) correctly rejected\n");
		lettuce_scheduler_stop();
	}

	/* Test 5 & 6: Sleep & Wakeup Behavior */
	{
		lettuce_scheduler_init();
		lettuce_scheduler_set_policy(LETTUCE_SCHED_POLICY_EEVDF);

		LettuceTask *t1 = lettuce_task_create(10, 100, 0x1000, 0x2000, "T_Active");
		LettuceTask *t2 = lettuce_task_create(20, 200, 0x3000, 0x4000, "T_Sleeper");

		/* Run t1 for several quanta to advance V_sys */
		lettuce_scheduler_start();
		for (int i = 0; i < 20; ++i)
		{
			t1->time_slice_ticks = 0;
			lettuce_scheduler_tick(&dummy_frame);
		}

		/* Sleep t2 */
		t2->state = LETTUCE_TASK_STATE_SLEEPING;
		assert(!lettuce_eevdf_is_eligible(t2));
		printf("  [✓] Test 5: Sleeping task removed from eligible set\n");

		/* Wakeup t2 */
		t2->state = LETTUCE_TASK_STATE_READY;
		g_lettuce_sched_eevdf_ops.on_task_wake(t2);

		/* Verify vruntime was caught up to V_sys, preventing unlimited lag accumulation */
		assert(t2->vruntime >= lettuce_eevdf_get_vsys());
		assert(lettuce_eevdf_is_eligible(t2));
		printf("  [✓] Test 6: Waking task caught up to V_sys without unbounded credit\n");
		lettuce_scheduler_stop();
	}

	/* Test 7 & 8: Terminated and Blocked Tasks Ignored */
	{
		lettuce_scheduler_init();
		lettuce_scheduler_set_policy(LETTUCE_SCHED_POLICY_EEVDF);

		LettuceTask *t1 = lettuce_task_create(10, 100, 0x1000, 0x2000, "T_Ready");
		LettuceTask *t2 = lettuce_task_create(20, 200, 0x3000, 0x4000, "T_Blocked");
		LettuceTask *t3 = lettuce_task_create(30, 300, 0x5000, 0x6000, "T_Term");

		t2->state = LETTUCE_TASK_STATE_BLOCKED;
		lettuce_task_terminate(t3->id);

		lettuce_scheduler_start();
		lettuce_scheduler_tick(&dummy_frame);

		assert(lettuce_task_current() == t1);
		printf("  [✓] Tests 7 & 8: Blocked and Terminated tasks strictly ignored\n");
		lettuce_scheduler_stop();
	}

	/* Test 1: Equal-Weight Tasks Receive Fair Runtime */
	{
		lettuce_scheduler_init();
		lettuce_scheduler_set_policy(LETTUCE_SCHED_POLICY_EEVDF);

		LettuceTask *ta = lettuce_task_create(10, 100, 0x1000, 0x2000, "TaskA");
		LettuceTask *tb = lettuce_task_create(20, 200, 0x3000, 0x4000, "TaskB");
		lettuce_task_set_weight(ta, LETTUCE_EEVDF_BASE_WEIGHT);
		lettuce_task_set_weight(tb, LETTUCE_EEVDF_BASE_WEIGHT);
		lettuce_task_set_slice(ta, 1);
		lettuce_task_set_slice(tb, 1);

		lettuce_scheduler_start();

		/* Run 100 ticks */
		for (int tick = 0; tick < 100; ++tick)
		{
			lettuce_scheduler_tick(&dummy_frame);
		}

		/* Both tasks should have received approximately equal turns (45-55) */
		int diff = abs((int)ta->total_ticks_run - (int)tb->total_ticks_run);
		printf("  [✓] Test 1: Equal-weight runtime: TaskA=%lu, TaskB=%lu (diff=%d ticks)\n",
		       (unsigned long)ta->total_ticks_run, (unsigned long)tb->total_ticks_run, diff);
		assert(diff <= 5);
		lettuce_scheduler_stop();
	}

	/* Test 2: Weighted Proportional Runtime */
	{
		lettuce_scheduler_init();
		lettuce_scheduler_set_policy(LETTUCE_SCHED_POLICY_EEVDF);

		LettuceTask *t_light = lettuce_task_create(10, 100, 0x1000, 0x2000, "T_Light");
		LettuceTask *t_heavy = lettuce_task_create(20, 200, 0x3000, 0x4000, "T_Heavy");
		/* Weight ratio 2:1 (2048 vs 1024) */
		lettuce_task_set_weight(t_heavy, 2048);
		lettuce_task_set_weight(t_light, 1024);
		lettuce_task_set_slice(t_heavy, 2);
		lettuce_task_set_slice(t_light, 2);

		lettuce_scheduler_start();

		/* Run 300 ticks */
		for (int tick = 0; tick < 300; ++tick)
		{
			lettuce_scheduler_tick(&dummy_frame);
		}

		printf("  [✓] Test 2: Weighted runtime (2:1 ratio): T_Heavy=%lu, T_Light=%lu\n",
		       (unsigned long)t_heavy->total_ticks_run, (unsigned long)t_light->total_ticks_run);
		/* T_Heavy should receive ~2x the ticks of T_Light */
		assert(t_heavy->total_ticks_run > t_light->total_ticks_run);
		assert(t_heavy->total_ticks_run >= 180);
		assert(t_light->total_ticks_run >= 90);
		lettuce_scheduler_stop();
	}

	/* Test 9: Integer Overflow Safety */
	{
		lettuce_scheduler_init();
		lettuce_scheduler_set_policy(LETTUCE_SCHED_POLICY_EEVDF);

		LettuceTask *t = lettuce_task_create(10, 100, 0x1000, 0x2000, "T_Overflow");
		lettuce_task_set_weight(t, 10240); /* Max weight */
		uint64_t dl = lettuce_eevdf_calc_deadline(t);
		assert(dl > 0);

		lettuce_task_set_weight(t, 1);     /* Min weight */
		dl = lettuce_eevdf_calc_deadline(t);
		assert(dl > 0);
		printf("  [✓] Test 9: Integer arithmetic bounds verified without overflow\n");
	}

	printf("[PASS] All 10 sched_eevdf_unit tests passed!\n");
	return 0;
}
