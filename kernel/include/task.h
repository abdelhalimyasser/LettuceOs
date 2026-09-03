/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/include/task.h
 *
 * Purpose:
 *   Defines bounded task records and task-state interfaces used by the
 *   scheduler and architecture handoff code.
 *
 * Design:
 *   Task storage is statically bounded and records include the domain state
 *   required for an architecture-specific address-space transition.
 */

#ifndef LETTUCE_TASK_H
#define LETTUCE_TASK_H

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <lettuce/types.h>

#define LETTUCE_MAX_TASKS 16u

/* EEVDF Weight Standards (Base Weight = 1024) */
#define LETTUCE_EEVDF_BASE_WEIGHT     1024u
#define LETTUCE_EEVDF_WEIGHT_LOW       512u
#define LETTUCE_EEVDF_WEIGHT_NORMAL   1024u
#define LETTUCE_EEVDF_WEIGHT_HIGH     2048u
#define LETTUCE_EEVDF_DEFAULT_SLICE      5u

typedef uint32_t LettuceTaskId;

typedef enum LettuceTaskState {
	LETTUCE_TASK_STATE_UNUSED = 0,
	LETTUCE_TASK_STATE_READY,
	LETTUCE_TASK_STATE_RUNNING,
	LETTUCE_TASK_STATE_SLEEPING,
	LETTUCE_TASK_STATE_BLOCKED,
	LETTUCE_TASK_STATE_TERMINATED
} LettuceTaskState;

typedef struct LettuceTaskContext {
	uint64_t x[31];       /* x0 - x30 (x30 is LR) */
	uint64_t sp_el0;      /* User stack pointer */
	uint64_t elr_el1;     /* User instruction pointer */
	uint64_t spsr_el1;    /* Saved PSTATE */
} LettuceTaskContext;

typedef struct LettuceTask {
	/* Hot identification & scheduling fields */
	LettuceTaskId id;                /* offset 0, size 4 [31:16 gen | 15:0 slot] */
	uint16_t generation;             /* offset 4, size 2 */
	uint8_t priority;                /* offset 6, size 1 */
	uint8_t padding1;                /* offset 7, size 1 (explicit padding) */
	LettuceServiceId service_id;     /* offset 8, size 4 */
	LettuceDomainId domain_id;       /* offset 12, size 4 */
	LettuceTaskState state;          /* offset 16, size 4 */
	uint32_t weight;                 /* offset 20, size 4 (EEVDF weight, base 1024) */
	uint32_t requested_slice_ticks;  /* offset 24, size 4 (EEVDF requested slice) */
	uint32_t padding2;               /* offset 28, size 4 (explicit alignment padding) */
	uint64_t time_slice_ticks;       /* offset 32, size 8 (remaining slice) */
	uint64_t quantum_ticks;          /* offset 40, size 8 (quantum in ticks) */
	uint64_t vruntime;               /* offset 48, size 8 (virtual runtime) */
	uint64_t vdeadline;              /* offset 56, size 8 (virtual deadline) */
	uint64_t sleep_deadline_ticks;   /* offset 64, size 8 (wakeup tick) */
	uint64_t total_ticks_run;        /* offset 72, size 8 (execution counter) */

	/* Cold / debugging fields */
	uintptr_t stack_base;            /* offset 80, size 8 */
	size_t stack_size;               /* offset 88, size 8 */
	const char *name;                /* offset 96, size 8 */

	/* Context (272 bytes) */
	LettuceTaskContext context;      /* offset 104, size 272 */
} LettuceTask;

_Static_assert(sizeof(LettuceTask) == 376, "LettuceTask layout must be exactly 376 bytes");
_Static_assert(_Alignof(LettuceTask) == 8, "LettuceTask must be 8-byte aligned");

void lettuce_task_init_subsystem(void);
LettuceTask *lettuce_task_create(
	LettuceServiceId service_id,
	LettuceDomainId domain_id,
	uintptr_t entry_pc,
	uintptr_t stack_top,
	const char *name);

LettuceTask *lettuce_task_get(LettuceTaskId id);
LettuceTask *lettuce_task_current(void);
void lettuce_task_set_current(LettuceTask *task);
void lettuce_task_terminate(LettuceTaskId id);
uint32_t lettuce_task_active_count(void);

void lettuce_task_set_weight(LettuceTask *task, uint32_t weight);
uint32_t lettuce_task_get_weight(const LettuceTask *task);
void lettuce_task_set_slice(LettuceTask *task, uint32_t slice_ticks);
uint32_t lettuce_task_get_slice(const LettuceTask *task);

#endif /* LETTUCE_TASK_H */
