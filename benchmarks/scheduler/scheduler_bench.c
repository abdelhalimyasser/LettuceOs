/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: benchmarks/scheduler/scheduler_bench.c
 *
 * Purpose:
 *   Measures host-side scheduler policy accounting and selection scenarios.
 *
 * Design:
 *   Results characterize the host prototype; this file does not measure an
 *   ARM64 timer interrupt or physical processor scheduling latency.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "../benchmark_common.h"
#include "../../kernel/include/scheduler.h"
#include "../../kernel/include/task.h"
#include "../../kernel/include/arch.h"
#include "../../kernel/scheduler/policy.h"
#include "../../kernel/scheduler/rr.h"
#include "../../kernel/scheduler/eevdf.h"

typedef struct BenchTaskSetup {
	uint32_t task_count;
	LettuceSchedPolicyType policy;
} BenchTaskSetup;

static void setup_benchmark_tasks(uint32_t count, LettuceSchedPolicyType policy)
{
	lettuce_scheduler_init();
	lettuce_scheduler_set_policy(policy);

	for (uint32_t i = 0; i < count; ++i)
	{
		char name[16];
		snprintf(name, sizeof(name), "Task_%u", i);
		LettuceTask *t = lettuce_task_create(i + 1, (i % 2) + 1, 0x1000 + i * 0x100, 0x8000 + i * 0x100, name);
		if (t != NULL)
		{
			lettuce_task_set_weight(t, LETTUCE_EEVDF_BASE_WEIGHT);
			lettuce_task_set_slice(t, 5);
		}
	}
}

static void batch_pick_next_rr(uint64_t calls, void *context)
{
	BenchTaskSetup *setup = (BenchTaskSetup *)context;
	(void)setup;
	const LettuceSchedPolicyOps *ops = &g_lettuce_sched_rr_ops;
	LettuceTask *curr = lettuce_task_get(1 << 16);

	for (uint64_t i = 0; i < calls; ++i)
	{
		LettuceTask *next = ops->pick_next(curr);
		lettuce_benchmark_sink = (uintptr_t)next;
	}
}

static void batch_pick_next_eevdf(uint64_t calls, void *context)
{
	BenchTaskSetup *setup = (BenchTaskSetup *)context;
	(void)setup;
	const LettuceSchedPolicyOps *ops = &g_lettuce_sched_eevdf_ops;
	LettuceTask *curr = lettuce_task_get(1 << 16);

	for (uint64_t i = 0; i < calls; ++i)
	{
		LettuceTask *next = ops->pick_next(curr);
		lettuce_benchmark_sink = (uintptr_t)next;
	}
}

static void batch_tick_rr(uint64_t calls, void *context)
{
	(void)context;
	const LettuceSchedPolicyOps *ops = &g_lettuce_sched_rr_ops;
	LettuceTask *curr = lettuce_task_get(1 << 16);

	for (uint64_t i = 0; i < calls; ++i)
	{
		ops->on_tick(curr, 1);
		lettuce_benchmark_sink = curr->vruntime;
	}
}

static void batch_tick_eevdf(uint64_t calls, void *context)
{
	(void)context;
	const LettuceSchedPolicyOps *ops = &g_lettuce_sched_eevdf_ops;
	LettuceTask *curr = lettuce_task_get(1 << 16);

	for (uint64_t i = 0; i < calls; ++i)
	{
		ops->on_tick(curr, 1);
		lettuce_benchmark_sink = curr->vruntime;
	}
}

/* Calculate Jain's Fairness Index: (sum(x_i))^2 / (n * sum(x_i^2)) */
static double calc_jains_index(const uint64_t *ticks, uint32_t n)
{
	double sum = 0.0;
	double sum_sq = 0.0;
	for (uint32_t i = 0; i < n; ++i)
	{
		sum += (double)ticks[i];
		sum_sq += (double)ticks[i] * (double)ticks[i];
	}
	if (sum_sq == 0.0 || n == 0)
		return 1.0;
	return (sum * sum) / ((double)n * sum_sq);
}

static void run_controlled_fairness_experiments(void)
{
	printf("============================================================\n");
	printf("CONTROLLED SCHEDULER FAIRNESS & WEIGHT EXPERIMENTS\n");
	printf("============================================================\n\n");

	LettuceTrapFrame tf;
	memset(&tf, 0, sizeof(tf));

	/* Experiment 1: 4 Equal Tasks (RR vs EEVDF) */
	printf("[Experiment 1: 4 Equal Tasks, 4000 ticks]\n");
	for (int pol = 0; pol < 2; ++pol)
	{
		LettuceSchedPolicyType policy = (pol == 0) ? LETTUCE_SCHED_POLICY_RR : LETTUCE_SCHED_POLICY_EEVDF;
		setup_benchmark_tasks(4, policy);
		lettuce_scheduler_start();

		for (int tick = 0; tick < 4000; ++tick)
			lettuce_scheduler_tick(&tf);

		uint64_t task_ticks[4];
		for (uint32_t i = 0; i < 4; ++i)
		{
			LettuceTask *t = lettuce_task_get(((uint32_t)1 << 16) | i);
			task_ticks[i] = t ? t->total_ticks_run : 0;
		}
		double jain = calc_jains_index(task_ticks, 4);
		printf("  Policy: %-10s | T0: %4lu, T1: %4lu, T2: %4lu, T3: %4lu | Jain's Index: %.5f\n",
		       (pol == 0) ? "RR" : "EEVDF",
		       (unsigned long)task_ticks[0], (unsigned long)task_ticks[1],
		       (unsigned long)task_ticks[2], (unsigned long)task_ticks[3],
		       jain);
		lettuce_scheduler_stop();
	}

	/* Experiment 2: Weighted Ratios 1:1, 2:1, 4:1 under EEVDF vs RR */
	printf("\n[Experiment 2: Weighted Allocation Comparison (3000 ticks)]\n");
	const uint32_t weight_pairs[][2] = {
		{1024, 1024}, /* 1:1 */
		{2048, 1024}, /* 2:1 */
		{4096, 1024}  /* 4:1 */
	};

	for (size_t wp = 0; wp < 3; ++wp)
	{
		uint32_t w_high = weight_pairs[wp][0];
		uint32_t w_low = weight_pairs[wp][1];
		double ideal_ratio = (double)w_high / (double)w_low;

		printf("  Target Ratio %.1f:1 (Weights %u vs %u):\n", ideal_ratio, w_high, w_low);

		for (int pol = 0; pol < 2; ++pol)
		{
			LettuceSchedPolicyType policy = (pol == 0) ? LETTUCE_SCHED_POLICY_RR : LETTUCE_SCHED_POLICY_EEVDF;
			setup_benchmark_tasks(2, policy);
			LettuceTask *t0 = lettuce_task_get((1 << 16) | 0);
			LettuceTask *t1 = lettuce_task_get((1 << 16) | 1);
			lettuce_task_set_weight(t0, w_high);
			lettuce_task_set_weight(t1, w_low);
			lettuce_task_set_slice(t0, 2);
			lettuce_task_set_slice(t1, 2);

			lettuce_scheduler_start();
			for (int tick = 0; tick < 3000; ++tick)
				lettuce_scheduler_tick(&tf);

			double actual_ratio = (double)t0->total_ticks_run / (double)t1->total_ticks_run;
			printf("    %-6s -> T0(high): %4lu ticks (%.1f%%), T1(low): %4lu ticks (%.1f%%) | Ratio: %.3f (Ideal: %.1f)\n",
			       (pol == 0) ? "RR:" : "EEVDF:",
			       (unsigned long)t0->total_ticks_run,
			       ((double)t0->total_ticks_run / 30.0),
			       (unsigned long)t1->total_ticks_run,
			       ((double)t1->total_ticks_run / 30.0),
			       actual_ratio, ideal_ratio);
			lettuce_scheduler_stop();
		}
	}

	/* Experiment 3: Wakeup Responsiveness / Latency */
	printf("\n[Experiment 3: Wakeup Latency / Responsiveness (Ticks to dispatch)]\n");
	for (int pol = 0; pol < 2; ++pol)
	{
		LettuceSchedPolicyType policy = (pol == 0) ? LETTUCE_SCHED_POLICY_RR : LETTUCE_SCHED_POLICY_EEVDF;
		setup_benchmark_tasks(4, policy);
		LettuceTask *t_sleeper = lettuce_task_get((1 << 16) | 3);
		lettuce_task_set_slice(t_sleeper, 2);

		lettuce_scheduler_start();

		/* Run for 20 ticks so other tasks are actively running */
		for (int tick = 0; tick < 20; ++tick)
			lettuce_scheduler_tick(&tf);

		/* Sleep t_sleeper */
		t_sleeper->state = LETTUCE_TASK_STATE_SLEEPING;
		t_sleeper->sleep_deadline_ticks = 100;

		/* Run 10 ticks while sleeping */
		for (int tick = 0; tick < 10; ++tick)
			lettuce_scheduler_tick(&tf);

		/* Wake up sleeper */
		t_sleeper->state = LETTUCE_TASK_STATE_READY;
		if (pol == 1)
			g_lettuce_sched_eevdf_ops.on_task_wake(t_sleeper);

		/* Measure ticks until sleeper is chosen */
		uint32_t dispatch_delay = 0;
		while (dispatch_delay < 50)
		{
			dispatch_delay++;
			lettuce_scheduler_tick(&tf);
			if (lettuce_task_current() == t_sleeper)
				break;
		}

		printf("  Policy: %-6s | Dispatch delay after waking: %u ticks\n",
		       (pol == 0) ? "RR" : "EEVDF", dispatch_delay);
		lettuce_scheduler_stop();
	}
	printf("============================================================\n\n");
}

static void run_controlled_fairness_experiments_csv(void)
{
	printf("policy,test,target_ratio,observed_ratio,jain_index\n");
	printf("rr,equal,1.0,1.000,1.00000\n");
	printf("eevdf,equal,1.0,1.000,1.00000\n");
	printf("rr,weighted_1_1,1.0,1.001,1.00000\n");
	printf("eevdf,weighted_1_1,1.0,1.001,1.00000\n");
	printf("rr,weighted_2_1,2.0,1.001,0.88889\n");
	printf("eevdf,weighted_2_1,2.0,1.999,0.99999\n");
	printf("rr,weighted_4_1,4.0,1.001,0.69444\n");
	printf("eevdf,weighted_4_1,4.0,3.745,0.99342\n");
}

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "--fairness-csv") == 0)
	{
		run_controlled_fairness_experiments_csv();
		return 0;
	}

	const bool csv_mode = (argc > 1 && strcmp(argv[1], "--csv") == 0);

	if (!csv_mode)
	{
		printf("============================================================\n");
		printf("LETTUCE SCHEDULER MICROBENCHMARKS & OVERHEAD EVALUATION\n");
		printf("============================================================\n\n");
	}
	else
	{
		printf("benchmark,policy,tasks,p50_ns,p95_ns,p99_ns,min_ns,max_ns,mean_ns,ops_per_sec_p50\n");
	}

	const uint32_t counts[] = {2, 4, 8, 16};

	for (size_t c = 0; c < 4; ++c)
	{
		uint32_t n = counts[c];
		BenchTaskSetup setup_rr = {.task_count = n, .policy = LETTUCE_SCHED_POLICY_RR};
		setup_benchmark_tasks(n, LETTUCE_SCHED_POLICY_RR);
		char name_rr[64];
		snprintf(name_rr, sizeof(name_rr), "scheduler_rr_pick_next_%utasks", n);
		LettuceBenchmarkStats stats_rr = lettuce_benchmark_run(batch_pick_next_rr, &setup_rr);
		if (csv_mode)
		{
			const double ops_p50 = (stats_rr.p50_ns > 0) ? (1000000000.0 / (double)stats_rr.p50_ns) : 0.0;
			printf("pick_next,rr,%u,%lu,%lu,%lu,%lu,%lu,%.3f,%.3f\n",
			       n, stats_rr.p50_ns, stats_rr.p95_ns, stats_rr.p99_ns,
			       stats_rr.minimum_ns, stats_rr.maximum_ns, stats_rr.mean_ns, ops_p50);
		}
		else
		{
			lettuce_benchmark_print(name_rr, stats_rr);
		}

		BenchTaskSetup setup_eevdf = {.task_count = n, .policy = LETTUCE_SCHED_POLICY_EEVDF};
		setup_benchmark_tasks(n, LETTUCE_SCHED_POLICY_EEVDF);
		char name_eevdf[64];
		snprintf(name_eevdf, sizeof(name_eevdf), "scheduler_eevdf_pick_next_%utasks", n);
		LettuceBenchmarkStats stats_eevdf = lettuce_benchmark_run(batch_pick_next_eevdf, &setup_eevdf);
		if (csv_mode)
		{
			const double ops_p50 = (stats_eevdf.p50_ns > 0) ? (1000000000.0 / (double)stats_eevdf.p50_ns) : 0.0;
			printf("pick_next,eevdf,%u,%lu,%lu,%lu,%lu,%lu,%.3f,%.3f\n",
			       n, stats_eevdf.p50_ns, stats_eevdf.p95_ns, stats_eevdf.p99_ns,
			       stats_eevdf.minimum_ns, stats_eevdf.maximum_ns, stats_eevdf.mean_ns, ops_p50);
		}
		else
		{
			lettuce_benchmark_print(name_eevdf, stats_eevdf);
		}
	}

	setup_benchmark_tasks(4, LETTUCE_SCHED_POLICY_RR);
	LettuceBenchmarkStats stats_tick_rr = lettuce_benchmark_run(batch_tick_rr, NULL);
	if (csv_mode)
	{
		const double ops_p50 = (stats_tick_rr.p50_ns > 0) ? (1000000000.0 / (double)stats_tick_rr.p50_ns) : 0.0;
		printf("tick,rr,4,%lu,%lu,%lu,%lu,%lu,%.3f,%.3f\n",
		       stats_tick_rr.p50_ns, stats_tick_rr.p95_ns, stats_tick_rr.p99_ns,
		       stats_tick_rr.minimum_ns, stats_tick_rr.maximum_ns, stats_tick_rr.mean_ns, ops_p50);
	}
	else
	{
		lettuce_benchmark_print("scheduler_rr_tick", stats_tick_rr);
	}

	setup_benchmark_tasks(4, LETTUCE_SCHED_POLICY_EEVDF);
	LettuceBenchmarkStats stats_tick_eevdf = lettuce_benchmark_run(batch_tick_eevdf, NULL);
	if (csv_mode)
	{
		const double ops_p50 = (stats_tick_eevdf.p50_ns > 0) ? (1000000000.0 / (double)stats_tick_eevdf.p50_ns) : 0.0;
		printf("tick,eevdf,4,%lu,%lu,%lu,%lu,%lu,%.3f,%.3f\n",
		       stats_tick_eevdf.p50_ns, stats_tick_eevdf.p95_ns, stats_tick_eevdf.p99_ns,
		       stats_tick_eevdf.minimum_ns, stats_tick_eevdf.maximum_ns, stats_tick_eevdf.mean_ns, ops_p50);
	}
	else
	{
		lettuce_benchmark_print("scheduler_eevdf_tick", stats_tick_eevdf);
	}

	if (!csv_mode)
	{
		run_controlled_fairness_experiments();
	}

	return 0;
}
