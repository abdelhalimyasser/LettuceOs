/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/include/arch.h
 *
 * Purpose:
 *   Declares architecture-neutral hooks used by kernel mechanisms to enter
 *   protection domains and perform architecture initialization.
 *
 * Design:
 *   Implementations own architecture-specific register and mapping mechanics;
 *   callers retain policy and authorization responsibility.
 */

#ifndef LETTUCE_ARCH_H
#define LETTUCE_ARCH_H

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <lettuce/types.h>

void lettuce_arch_init(void);
uint64_t lettuce_arch_current_el(void);
uint64_t lettuce_arch_counter_read(void);
uint64_t lettuce_arch_counter_frequency(void);
void lettuce_arch_console_putc(char character);
void lettuce_arch_console_puts(const char *text);
void lettuce_arch_console_print_hex(uint64_t value);
void lettuce_arch_console_print_dec(uint64_t value);
void lettuce_arch_exception_unhandled(void);

extern void lettuce_exception_vectors(void);

void lettuce_mmu_init(void);
LettuceDomainId lettuce_mmu_enter(LettuceDomainId domain);
void lettuce_mmu_leave(LettuceDomainId previous_domain);
LettuceDomainId lettuce_mmu_current_domain(void);
uint16_t lettuce_mmu_active_asid(void);
uint64_t lettuce_mmu_active_ttbr0(void);
void lettuce_mmu_invalidate_asid(uint16_t asid);
void lettuce_mmu_invalidate_domain(LettuceDomainId domain);
void lettuce_mmu_invalidate_all(void);

void lettuce_mmu_expect_data_abort(void);
void lettuce_mmu_expect_fault(uint64_t expected_far);
bool lettuce_mmu_expected_fault_observed(void);
bool lettuce_mmu_handle_expected_data_abort(void);
bool lettuce_mmu_handle_fault(uint64_t esr, uint64_t far, uint64_t elr);
uint64_t lettuce_mmu_last_fault_far(void);
uint64_t lettuce_mmu_last_fault_esr(void);
uint64_t lettuce_mmu_last_fault_elr(void);

uintptr_t lettuce_arch_domain_stack_top(LettuceDomainId domain);
void lettuce_arch_expect_sysreg_trap(void);
bool lettuce_arch_sysreg_trap_observed(void);
bool lettuce_arch_handle_sysreg_trap(uint64_t esr, uint64_t elr);

/* Minimal SVC ABI numbers */
#define LETTUCE_SVC_RETURN      0x00u /* x0 = LettuceStatus */
#define LETTUCE_SVC_CALL        0x01u /* x0 = target_service, x1 = op, x2 = res, x3 = cap */
#define LETTUCE_SVC_CROSS_LAYER 0x02u /* x0 = target_service, x1 = op, x2 = res, x3 = cap */
#define LETTUCE_SVC_ELEVATOR    0x03u /* x0 = target_service, x1 = op, x2 = res, x3 = cap */
#define LETTUCE_SVC_YIELD       0x04u /* cooperative yield */
#define LETTUCE_SVC_SYSCALL     0x05u /* x8 = sys_no, x0-x5 = args */

#define LETTUCE_SYS_WRITE         1u
#define LETTUCE_SYS_READ          2u
#define LETTUCE_SYS_CLOSE         3u
#define LETTUCE_SYS_GETPID        4u
#define LETTUCE_SYS_CLOCK_GETTIME 5u
#define LETTUCE_SYS_NANOSLEEP     6u

uint64_t lettuce_arch_domain_ttbr0_val(LettuceDomainId domain);
void lettuce_mmu_set_current_domain(LettuceDomainId domain);

typedef struct LettuceTrapFrame {
	uint64_t x[30]; /* x0 - x29 */
	uint64_t lr;    /* x30 */
	uint64_t elr;
	uint64_t spsr;
	uint64_t sp_el0;
} LettuceTrapFrame;

_Static_assert(sizeof(LettuceTrapFrame) == 272, "LettuceTrapFrame must remain exactly 272 bytes.");
_Static_assert(_Alignof(LettuceTrapFrame) == 8, "LettuceTrapFrame must be 8-byte aligned.");

int64_t lettuce_el0_enter(uintptr_t entry_pc, uintptr_t el0_sp);
void lettuce_el0_resume_kernel(int64_t status);

typedef struct LettuceArchContextFrame {
	LettuceServiceId service_id;
	LettuceDomainId domain_id;
	uint64_t elr;
	uint64_t spsr;
	uint64_t sp_el0;
} LettuceArchContextFrame;

_Static_assert(sizeof(LettuceArchContextFrame) == 32, "LettuceArchContextFrame must remain exactly 32 bytes.");
_Static_assert(_Alignof(LettuceArchContextFrame) == 8, "LettuceArchContextFrame must be 8-byte aligned.");
_Static_assert(offsetof(LettuceArchContextFrame, elr) == 8, "elr must reside at offset 8.");

bool lettuce_arch_context_push(LettuceServiceId svc, LettuceDomainId dom, uint64_t elr, uint64_t spsr, uint64_t sp_el0);
bool lettuce_arch_context_pop(LettuceArchContextFrame *out_frame);
uint32_t lettuce_arch_context_depth(void);

void lettuce_pac_init(void);
uint64_t lettuce_pac_sign(uint64_t ptr, uint64_t modifier);
uint64_t lettuce_pac_auth(uint64_t ptr, uint64_t modifier);
void lettuce_pac_enable(bool enable);
bool lettuce_pac_is_enabled(void);
void lettuce_pac_expect_trap(void);
bool lettuce_pac_trap_observed(void);
void lettuce_arch_context_corrupt_top_elr(uint64_t xor_mask);

#endif
