/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/arch/arm64/exception.S
 *
 * Purpose:
 *   Defines ARM64 EL1 exception-vector entries and trap-frame transitions.
 *
 * Flow:
 *   EL0 or EL1 exception -> register save -> C handler -> register restore ->
 *   ERET or architecture-defined return path.
 *
 * Key invariants:
 *   Trap-frame layout and return state must remain synchronized with C code.
 */

.section .text.exception, "ax"
.balign 2048
.global lettuce_exception_vectors
.type lettuce_exception_vectors, %function
.extern lettuce_arch_sync_el1_handler
.extern lettuce_arch_sync_lower_el_handler
.extern lettuce_arch_irq_el1_handler
.extern lettuce_arch_irq_lower_el_handler
.extern lettuce_arch_exception_unexpected

lettuce_exception_vectors:

/* 0x000: Current EL with SP0 Sync */
.balign 128
	b vector_unexpected
/* 0x080: Current EL with SP0 IRQ */
.balign 128
	b vector_unexpected
/* 0x100: Current EL with SP0 FIQ */
.balign 128
	b vector_unexpected
/* 0x180: Current EL with SP0 SError */
.balign 128
	b vector_unexpected

/* 0x200: Current EL with SPx Sync */
.balign 128
	b vector_sync_el1
/* 0x280: Current EL with SPx IRQ */
.balign 128
	b vector_irq_el1
/* 0x300: Current EL with SPx FIQ */
.balign 128
	b vector_unexpected
/* 0x380: Current EL with SPx SError */
.balign 128
	b vector_unexpected

/* 0x400: Lower EL AArch64 Sync */
.balign 128
	b vector_sync_lower_el
/* 0x480: Lower EL AArch64 IRQ */
.balign 128
	b vector_irq_lower_el
/* 0x500: Lower EL AArch64 FIQ */
.balign 128
	b vector_unexpected
/* 0x580: Lower EL AArch64 SError */
.balign 128
	b vector_unexpected

/* 0x600: Lower EL AArch32 Sync */
.balign 128
	b vector_unexpected
/* 0x680: Lower EL AArch32 IRQ */
.balign 128
	b vector_unexpected
/* 0x700: Lower EL AArch32 FIQ */
.balign 128
	b vector_unexpected
/* 0x780: Lower EL AArch32 SError */
.balign 128
	b vector_unexpected

vector_sync_el1:
	sub sp, sp, #272
	stp x0, x1, [sp, #0]
	stp x2, x3, [sp, #16]
	stp x4, x5, [sp, #32]
	stp x6, x7, [sp, #48]
	stp x8, x9, [sp, #64]
	stp x10, x11, [sp, #80]
	stp x12, x13, [sp, #96]
	stp x14, x15, [sp, #112]
	stp x16, x17, [sp, #128]
	stp x18, x19, [sp, #144]
	stp x20, x21, [sp, #160]
	stp x22, x23, [sp, #176]
	stp x24, x25, [sp, #192]
	stp x26, x27, [sp, #208]
	stp x28, x29, [sp, #224]
	str x30, [sp, #240]
	mrs x21, elr_el1
	mrs x22, spsr_el1
	stp x21, x22, [sp, #248]
	mrs x23, sp_el0
	str x23, [sp, #264]

	mov x0, sp
	bl lettuce_arch_sync_el1_handler

	ldr x23, [sp, #264]
	msr sp_el0, x23
	ldp x21, x22, [sp, #248]
	msr elr_el1, x21
	msr spsr_el1, x22
	ldp x0, x1, [sp, #0]
	ldp x2, x3, [sp, #16]
	ldp x4, x5, [sp, #32]
	ldp x6, x7, [sp, #48]
	ldp x8, x9, [sp, #64]
	ldp x10, x11, [sp, #80]
	ldp x12, x13, [sp, #96]
	ldp x14, x15, [sp, #112]
	ldp x16, x17, [sp, #128]
	ldp x18, x19, [sp, #144]
	ldp x20, x21, [sp, #160]
	ldp x22, x23, [sp, #176]
	ldp x24, x25, [sp, #192]
	ldp x26, x27, [sp, #208]
	ldp x28, x29, [sp, #224]
	ldr x30, [sp, #240]
	add sp, sp, #272
	eret

vector_sync_lower_el:
	sub sp, sp, #272
	stp x0, x1, [sp, #0]
	stp x2, x3, [sp, #16]
	stp x4, x5, [sp, #32]
	stp x6, x7, [sp, #48]
	stp x8, x9, [sp, #64]
	stp x10, x11, [sp, #80]
	stp x12, x13, [sp, #96]
	stp x14, x15, [sp, #112]
	stp x16, x17, [sp, #128]
	stp x18, x19, [sp, #144]
	stp x20, x21, [sp, #160]
	stp x22, x23, [sp, #176]
	stp x24, x25, [sp, #192]
	stp x26, x27, [sp, #208]
	stp x28, x29, [sp, #224]
	str x30, [sp, #240]
	mrs x21, elr_el1
	mrs x22, spsr_el1
	stp x21, x22, [sp, #248]
	mrs x23, sp_el0
	str x23, [sp, #264]

	mov x0, sp
	bl lettuce_arch_sync_lower_el_handler

	ldr x23, [sp, #264]
	msr sp_el0, x23
	ldp x21, x22, [sp, #248]
	msr elr_el1, x21
	msr spsr_el1, x22
	ldp x0, x1, [sp, #0]
	ldp x2, x3, [sp, #16]
	ldp x4, x5, [sp, #32]
	ldp x6, x7, [sp, #48]
	ldp x8, x9, [sp, #64]
	ldp x10, x11, [sp, #80]
	ldp x12, x13, [sp, #96]
	ldp x14, x15, [sp, #112]
	ldp x16, x17, [sp, #128]
	ldp x18, x19, [sp, #144]
	ldp x20, x21, [sp, #160]
	ldp x22, x23, [sp, #176]
	ldp x24, x25, [sp, #192]
	ldp x26, x27, [sp, #208]
	ldp x28, x29, [sp, #224]
	ldr x30, [sp, #240]
	add sp, sp, #272
	eret

vector_irq_el1:
	sub sp, sp, #272
	stp x0, x1, [sp, #0]
	stp x2, x3, [sp, #16]
	stp x4, x5, [sp, #32]
	stp x6, x7, [sp, #48]
	stp x8, x9, [sp, #64]
	stp x10, x11, [sp, #80]
	stp x12, x13, [sp, #96]
	stp x14, x15, [sp, #112]
	stp x16, x17, [sp, #128]
	stp x18, x19, [sp, #144]
	stp x20, x21, [sp, #160]
	stp x22, x23, [sp, #176]
	stp x24, x25, [sp, #192]
	stp x26, x27, [sp, #208]
	stp x28, x29, [sp, #224]
	str x30, [sp, #240]
	mrs x21, elr_el1
	mrs x22, spsr_el1
	stp x21, x22, [sp, #248]
	mrs x23, sp_el0
	str x23, [sp, #264]

	mov x0, sp
	bl lettuce_arch_irq_el1_handler

	ldr x23, [sp, #264]
	msr sp_el0, x23
	ldp x21, x22, [sp, #248]
	msr elr_el1, x21
	msr spsr_el1, x22
	ldp x0, x1, [sp, #0]
	ldp x2, x3, [sp, #16]
	ldp x4, x5, [sp, #32]
	ldp x6, x7, [sp, #48]
	ldp x8, x9, [sp, #64]
	ldp x10, x11, [sp, #80]
	ldp x12, x13, [sp, #96]
	ldp x14, x15, [sp, #112]
	ldp x16, x17, [sp, #128]
	ldp x18, x19, [sp, #144]
	ldp x20, x21, [sp, #160]
	ldp x22, x23, [sp, #176]
	ldp x24, x25, [sp, #192]
	ldp x26, x27, [sp, #208]
	ldp x28, x29, [sp, #224]
	ldr x30, [sp, #240]
	add sp, sp, #272
	eret

vector_irq_lower_el:
	sub sp, sp, #272
	stp x0, x1, [sp, #0]
	stp x2, x3, [sp, #16]
	stp x4, x5, [sp, #32]
	stp x6, x7, [sp, #48]
	stp x8, x9, [sp, #64]
	stp x10, x11, [sp, #80]
	stp x12, x13, [sp, #96]
	stp x14, x15, [sp, #112]
	stp x16, x17, [sp, #128]
	stp x18, x19, [sp, #144]
	stp x20, x21, [sp, #160]
	stp x22, x23, [sp, #176]
	stp x24, x25, [sp, #192]
	stp x26, x27, [sp, #208]
	stp x28, x29, [sp, #224]
	str x30, [sp, #240]
	mrs x21, elr_el1
	mrs x22, spsr_el1
	stp x21, x22, [sp, #248]
	mrs x23, sp_el0
	str x23, [sp, #264]

	mov x0, sp
	bl lettuce_arch_irq_lower_el_handler

	ldr x23, [sp, #264]
	msr sp_el0, x23
	ldp x21, x22, [sp, #248]
	msr elr_el1, x21
	msr spsr_el1, x22
	ldp x0, x1, [sp, #0]
	ldp x2, x3, [sp, #16]
	ldp x4, x5, [sp, #32]
	ldp x6, x7, [sp, #48]
	ldp x8, x9, [sp, #64]
	ldp x10, x11, [sp, #80]
	ldp x12, x13, [sp, #96]
	ldp x14, x15, [sp, #112]
	ldp x16, x17, [sp, #128]
	ldp x18, x19, [sp, #144]
	ldp x20, x21, [sp, #160]
	ldp x22, x23, [sp, #176]
	ldp x24, x25, [sp, #192]
	ldp x26, x27, [sp, #208]
	ldp x28, x29, [sp, #224]
	ldr x30, [sp, #240]
	add sp, sp, #272
	eret

vector_unexpected:
	bl lettuce_arch_exception_unexpected
	1: wfe
	b 1b
