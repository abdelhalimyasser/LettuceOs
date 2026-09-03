
/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/arch/arm64/elevator.S
 *
 * Purpose:
 *   Implements the specialized ARM64 Elevator transition after EL1 C code has
 *   completed authorization.
 *
 * Execution context:
 *   EL1 privileged transition path.
 *
 * Key invariants:
 *   - Assembly never decides authorization or bypasses capability policy.
 *   - Address-space handoff preserves the barrier ordering required by code.
 */

.section .text, "ax"

/*
 * void lettuce_elevator_asm_transition(const LettuceElevatorDescriptor *desc, void *trap_frame);
 *
 * x0 = pointer to const LettuceElevatorDescriptor
 *      offset 0:  target_entry_pc
 *      offset 8:  target_sp_el0
 *      offset 16: target_ttbr0_val
 *      offset 24: caller_ttbr0_val
 *      offset 32: target_service (32-bit)
 *      offset 36: target_domain (32-bit)
 *      offset 40: caller_service (32-bit)
 *      offset 44: caller_domain (32-bit)
 *
 * x1 = pointer to LettuceTrapFrame on kernel stack
 *      offset 248: elr
 *      offset 256: spsr
 *      offset 264: sp_el0
 */
.global lettuce_elevator_asm_transition
.type lettuce_elevator_asm_transition, %function
lettuce_elevator_asm_transition:
	/* 1. Direct hardware TTBR0_EL1 switch with target ASID */
	ldr x2, [x0, #16]      /* desc->target_ttbr0_val */
	dsb ish
	msr ttbr0_el1, x2
	isb

	/* 2. Direct installation of target EL0 entry PC and SP_EL0 */
	ldr x3, [x0, #0]       /* desc->target_entry_pc */
	ldr x4, [x0, #8]       /* desc->target_sp_el0 */
	str x3, [x1, #248]     /* frame->elr */
	str xzr, [x1, #256]    /* frame->spsr = 0 (EL0t) */
	str x4, [x1, #264]     /* frame->sp_el0 */

	ret
