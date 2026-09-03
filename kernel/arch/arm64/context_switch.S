/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/arch/arm64/context_switch.S
 *
 * Purpose:
 *   Implements the register-save and restore primitive used for ARM64 task
 *   context switching.
 *
 * Execution context:
 *   EL1 scheduler transition path.
 *
 * Key invariants:
 *   Required callee-saved register and stack state are preserved exactly.
 */

.section .text, "ax"

/*
 * void lettuce_arch_context_switch(LettuceTaskContext *prev, const LettuceTaskContext *next)
 *
 * x0 = prev context pointer
 * x1 = next context pointer
 *
 * Saves callee-saved registers and stack state into prev,
 * then restores them from next and returns to next's saved LR.
 */
.global lettuce_arch_context_switch
.type lettuce_arch_context_switch, %function
lettuce_arch_context_switch:
	cbz x0, 1f

	/* Save x19-x30 */
	stp x19, x20, [x0, #(19 * 8)]
	stp x21, x22, [x0, #(21 * 8)]
	stp x23, x24, [x0, #(23 * 8)]
	stp x25, x26, [x0, #(25 * 8)]
	stp x27, x28, [x0, #(27 * 8)]
	stp x29, x30, [x0, #(29 * 8)]

	/* Save current SP */
	mov x2, sp
	str x2, [x0, #(31 * 8)] /* sp_el0 or kernel sp offset */

1:
	cbz x1, 2f

	/* Restore x19-x30 */
	ldp x19, x20, [x1, #(19 * 8)]
	ldp x21, x22, [x1, #(21 * 8)]
	ldp x23, x24, [x1, #(23 * 8)]
	ldp x25, x26, [x1, #(25 * 8)]
	ldp x27, x28, [x1, #(27 * 8)]
	ldp x29, x30, [x1, #(29 * 8)]

	/* Restore SP */
	ldr x2, [x1, #(31 * 8)]
	mov sp, x2

2:
	ret
