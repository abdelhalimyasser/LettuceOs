/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/arch/arm64/boot.S
 *
 * Purpose:
 *   Supplies the ARM64 reset entry that establishes early execution state and
 *   transfers control to the EL1 C kernel entry.
 *
 * Execution context:
 *   Early privileged ARM64 boot code.
 *
 * This file does not:
 *   Perform service dispatch, capability authorization, or scheduling policy.
 */

.section .text.boot, "ax"
.global _start
.type _start, %function
.extern lettuce_arm64_main

_start:
	msr daifset, #0xf
	adrp x0, __bss_start
	add x0, x0, :lo12:__bss_start
	adrp x1, __bss_end
	add x1, x1, :lo12:__bss_end
	mov x2, xzr
1:
	cmp x0, x1
	b.hs 2f
	stp x2, x2, [x0], #16
	b 1b
2:
	adrp x0, boot_stack_top
	add sp, x0, :lo12:boot_stack_top
	bl lettuce_arm64_main
3:
	wfe
	b 3b

.section .bss.stack, "aw", %nobits
.balign 16
boot_stack:
	.skip 16384
boot_stack_top:
