/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/arch/arm64/irq.S
 *
 * Purpose:
 *   Provides the ARM64 IRQ assembly entry bridge to the EL1 C interrupt
 *   handler.
 *
 * Execution context:
 *   EL1 interrupt-entry code.
 *
 * Key invariants:
 *   Register preservation and interrupt return state must match the exception
 *   entry contract.
 */

.section .text, "ax"

.global lettuce_arch_irq_enable
.type lettuce_arch_irq_enable, %function
lettuce_arch_irq_enable:
	msr daifclr, #2
	isb
	ret

.global lettuce_arch_irq_disable
.type lettuce_arch_irq_disable, %function
lettuce_arch_irq_disable:
	msr daifset, #2
	isb
	ret

.global lettuce_arch_irq_save
.type lettuce_arch_irq_save, %function
lettuce_arch_irq_save:
	mrs x0, daif
	msr daifset, #2
	isb
	ret

.global lettuce_arch_irq_restore
.type lettuce_arch_irq_restore, %function
lettuce_arch_irq_restore:
	msr daif, x0
	isb
	ret
