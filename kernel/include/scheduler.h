/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/include/scheduler.h
 *
 * Purpose:
 *   Declares scheduler mechanism APIs, lifecycle operations, and policy
 *   selection controls.
 *
 * Design:
 *   The mechanism owns task lifecycle and switching coordination; policies
 *   select ready tasks without owning exception vectors or MMU state.
 */

#ifndef LETTUCE_SCHEDULER_H
#define LETTUCE_SCHEDULER_H

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "task.h"

typedef enum LettuceSchedPolicyType {
	LETTUCE_SCHED_POLICY_RR = 0,
	LETTUCE_SCHED_POLICY_EEVDF = 1
} LettuceSchedPolicyType;

void lettuce_scheduler_init(void);
void lettuce_scheduler_start(void);
void lettuce_scheduler_stop(void);
bool lettuce_scheduler_is_active(void);

void lettuce_scheduler_set_policy(LettuceSchedPolicyType policy);
LettuceSchedPolicyType lettuce_scheduler_get_policy(void);
const char *lettuce_scheduler_get_policy_name(void);

void lettuce_scheduler_tick(void *trap_frame);
void lettuce_scheduler_sleep_current(uint64_t ticks, void *trap_frame);
void lettuce_scheduler_yield(void *trap_frame);
void lettuce_scheduler_set_preempt_limit(uint64_t max_preempts);

uint64_t lettuce_scheduler_switch_count(void);
uint64_t lettuce_scheduler_preempt_count(void);
uint64_t lettuce_scheduler_same_domain_switches(void);
uint64_t lettuce_scheduler_cross_domain_switches(void);

#endif /* LETTUCE_SCHEDULER_H */
