/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/include/elevator_asm.h
 *
 * Purpose:
 *   Declares the narrow C-to-AArch64 interface for an authorized Elevator
 *   transition.
 *
 * Design:
 *   This interface exposes mechanics only; C policy must authorize every
 *   transition before assembly is entered.
 */

#ifndef LETTUCE_ELEVATOR_ASM_H
#define LETTUCE_ELEVATOR_ASM_H

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <lettuce/types.h>

typedef struct LettuceElevatorDescriptor {
	uint64_t target_entry_pc;
	uint64_t target_sp_el0;
	uint64_t target_ttbr0_val;
	uint64_t caller_ttbr0_val;
	LettuceServiceId target_service;
	LettuceDomainId target_domain;
	LettuceServiceId caller_service;
	LettuceDomainId caller_domain;
} LettuceElevatorDescriptor;

void lettuce_elevator_asm_transition(const LettuceElevatorDescriptor *desc, void *trap_frame);
void lettuce_elevator_set_use_asm(bool use_asm);
bool lettuce_elevator_is_using_asm(void);

#endif /* LETTUCE_ELEVATOR_ASM_H */
