/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/scheduler/policy.h
 *
 * Purpose:
 *   Defines the pluggable scheduler-policy contract used by the invariant
 *   scheduler mechanism.
 *
 * Design:
 *   Policies contribute ready-task selection and accounting, not IRQ handling,
 *   MMU/domain handoff, or exception vectors.
 */

#ifndef LETTUCE_SCHED_POLICY_H
#define LETTUCE_SCHED_POLICY_H

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../include/task.h"

/*
 * Internal Scheduler Policy Operations Interface.
 *
 * Each policy implementation decides strictly:
 * 1. Which READY task should be executed next.
 * 2. How scheduling metadata (vruntime, deadlines, quanta) is updated.
 *
 * Hardware MMU switching, IRQ management, trap frames, and register contexts
 * are strictly preserved in the common scheduler core.
 */
typedef struct LettuceSchedPolicyOps {
	const char *name;
	void (*init)(void);
	void (*on_tick)(LettuceTask *curr, uint64_t tick_delta);
	LettuceTask *(*pick_next)(LettuceTask *curr);
	void (*on_task_ready)(LettuceTask *task);
	void (*on_task_sleep)(LettuceTask *task);
	void (*on_task_wake)(LettuceTask *task);
	void (*on_task_block)(LettuceTask *task);
	void (*on_task_exit)(LettuceTask *task);
} LettuceSchedPolicyOps;

extern const LettuceSchedPolicyOps g_lettuce_sched_rr_ops;
extern const LettuceSchedPolicyOps g_lettuce_sched_eevdf_ops;

#endif /* LETTUCE_SCHED_POLICY_H */
