/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/arch/arm64/entry.S
 *
 * Purpose:
 *   Prepares ARM64 EL0 entry state and performs exception-return transitions.
 *
 * Execution context:
 *   EL1-to-EL0 architecture handoff.
 *
 * Key invariants:
 *   ELR_EL1 and SPSR_EL1 are prepared before ERET and user execution starts
 *   only with the intended stack and program counter.
 */

.section .data
.balign 16
.global g_el1_saved_sp
g_el1_saved_sp:
	.quad 0

.section .text, "ax"
.balign 16

/*
 * int64_t lettuce_el0_enter(uintptr_t entry_pc, uintptr_t el0_sp)
 *
 * Enters EL0 from EL1:
 * - Saves EL1 callee-saved registers (x19-x30) on current SP_EL1
 * - Records SP_EL1 in g_el1_saved_sp for continuation recovery
 * - Configures ELR_EL1 = x0, SP_EL0 = x1, SPSR_EL1 = 0x0 (EL0t, AArch64)
 * - Executes eret to drop into EL0
 *
 * Returns here when lettuce_el0_resume_kernel() is called upon SVC return or fault!
 * Return value in x0: status code.
 */
.global lettuce_el0_enter
.type lettuce_el0_enter, %function
lettuce_el0_enter:
	sub sp, sp, #96
	stp x19, x20, [sp, #0]
	stp x21, x22, [sp, #16]
	stp x23, x24, [sp, #32]
	stp x25, x26, [sp, #48]
	stp x27, x28, [sp, #64]
	stp x29, x30, [sp, #80]

	adrp x2, g_el1_saved_sp
	mov x3, sp
	str x3, [x2, :lo12:g_el1_saved_sp]

	msr elr_el1, x0
	msr sp_el0, x1
	msr spsr_el1, xzr
	isb

	eret

/*
 * void lettuce_el0_resume_kernel(int64_t status)
 *
 * Resumes EL1 continuation from an SVC handler or fault handler:
 * - Restores SP_EL1 from g_el1_saved_sp
 * - Restores EL1 callee-saved registers (x19-x30)
 * - Returns to the original caller of lettuce_el0_enter with status in x0
 */
.global lettuce_el0_resume_kernel
.type lettuce_el0_resume_kernel, %function
lettuce_el0_resume_kernel:
	adrp x2, g_el1_saved_sp
	ldr x3, [x2, :lo12:g_el1_saved_sp]
	mov sp, x3

	/* x0 holds the return status */
	ldp x19, x20, [sp, #0]
	ldp x21, x22, [sp, #16]
	ldp x23, x24, [sp, #32]
	ldp x25, x26, [sp, #48]
	ldp x27, x28, [sp, #64]
	ldp x29, x30, [sp, #80]
	add sp, sp, #96
	ret

/*
 * ============================================================================
 * SYNTHETIC EL0 SERVICES (.service_text section)
 * Mapped at SERVICE_CODE_VA (0x41050000) with EL0 Read-Only, Executable permissions.
 * ============================================================================
 */
.section .service_text, "ax"
.balign 4096

.global el0_service_code_start
el0_service_code_start:

/*
 * Service: Display (Service 20, Domain 200)
 * Writes to its own private page 0x41100000, verifies, returns OK via SVC #0.
 */
.global svc_display_entry
.type svc_display_entry, %function
svc_display_entry:
	ldr x1, =0x41100000
	ldr x2, =0x200b0001200b0001
	str x2, [x1]
	ldr x3, [x1]
	cmp x2, x3
	b.ne 1f
	mov x0, #0
	svc #0
1:
	mov x0, #1
	svc #0

/*
 * Service: Camera (Service 10, Domain 100)
 * Writes to its own private page 0x41000000, verifies, returns OK via SVC #0.
 */
.global svc_camera_entry
.type svc_camera_entry, %function
svc_camera_entry:
	ldr x1, =0x41000000
	ldr x2, =0x100a0001100a0001
	str x2, [x1]
	ldr x3, [x1]
	cmp x2, x3
	b.ne 1f
	mov x0, #0
	svc #0
1:
	mov x0, #1
	svc #0

/*
 * Service: Storage (Service 30, Domain 300)
 * Writes to its own private page 0x41180000, verifies, returns OK via SVC #0.
 */
.global svc_storage_entry
.type svc_storage_entry, %function
svc_storage_entry:
	ldr x1, =0x41180000
	ldr x2, =0x300c0001300c0001
	str x2, [x1]
	ldr x3, [x1]
	cmp x2, x3
	b.ne 1f
	mov x0, #0
	svc #0
1:
	mov x0, #1
	svc #0

/*
 * Failing Service: Returns LETTUCE_STATUS_ERROR (1) via SVC #0.
 */
.global svc_failing_entry
.type svc_failing_entry, %function
svc_failing_entry:
	mov x0, #1
	svc #0

/*
 * Camera Service calling Display Service via Same-Layer Gate:
 * 1. Writes to Camera private page 0x41000000
 * 2. Reads capability handle from shared page 0x411c0000
 * 3. Prepares registers: x0=20 (Display), x1=1 (OP_DRAW), x2=1 (RES_FB), x3=cap_handle
 * 4. Calls kernel gate via SVC #1 (LETTUCE_SVC_CALL)
 * 5. Checks return status in x0 (must be 0)
 * 6. Verifies Camera private page still intact
 * 7. Returns OK via SVC #0
 */
.global svc_camera_call_display
.type svc_camera_call_display, %function
svc_camera_call_display:
	ldr x1, =0x41000000
	ldr x2, =0x100a0001100a0001
	str x2, [x1]

	ldr x4, =0x411c0000
	ldr x3, [x4]

	mov x0, #20
	mov x1, #1
	mov x2, #1
	svc #1

	cbnz x0, 2f

	ldr x1, =0x41000000
	ldr x2, =0x100a0001100a0001
	ldr x3, [x1]
	cmp x2, x3
	b.ne 2f

	mov x0, #0
	svc #0
2:
	mov x0, #1
	svc #0

/*
 * Display Service calling Storage Service via Same-Layer Gate:
 * 1. Writes to Display private page 0x41100000
 * 2. Reads capability handle from shared page offset +8: 0x411c0008
 * 3. Prepares registers: x0=30 (Storage), x1=1 (OP_SAVE), x2=2 (RES_DISK), x3=cap
 * 4. Calls kernel gate via SVC #1 (LETTUCE_SVC_CALL)
 * 5. Checks return status in x0 (must be 0)
 * 6. Verifies Display private page still intact
 * 7. Returns OK via SVC #0
 */
.global svc_display_call_storage
.type svc_display_call_storage, %function
svc_display_call_storage:
	ldr x1, =0x41100000
	ldr x2, =0x200b0001200b0001
	str x2, [x1]

	ldr x4, =0x411c0008
	ldr x3, [x4]

	mov x0, #30
	mov x1, #1
	mov x2, #2
	svc #1

	cbnz x0, 5f

	ldr x1, =0x41100000
	ldr x2, =0x200b0001200b0001
	ldr x3, [x1]
	cmp x2, x3
	b.ne 5f

	mov x0, #0
	svc #0
5:
	mov x0, #1
	svc #0

/*
 * Security Negative Test: Privileged System Register Access from EL0
 * Tries to read SCTLR_EL1. Hardware must raise Trapped SysReg (EC=0x18).
 */
.global svc_test_sysreg
.type svc_test_sysreg, %function
svc_test_sysreg:
	mrs x0, sctlr_el1
	mov x0, #0
	svc #0

/*
 * Security Negative Test: Kernel Memory Access from EL0
 * Tries to read kernel text/data at 0x40082000. Hardware must raise Permission Fault (EC=0x24).
 */
.global svc_test_kernel_mem
.type svc_test_kernel_mem, %function
svc_test_kernel_mem:
	ldr x1, =0x40082000
	ldr x0, [x1]
	mov x0, #0
	svc #0

/*
 * Security Negative Test: Foreign Domain B Memory Access from Domain A EL0
 * Tries to read 0x41100000. Hardware must raise Translation Fault (EC=0x24).
 */
.global svc_test_foreign_mem_a_to_b
.type svc_test_foreign_mem_a_to_b, %function
svc_test_foreign_mem_a_to_b:
	ldr x1, =0x41100000
	ldr x0, [x1]
	mov x0, #0
	svc #0

/*
 * Security Negative Test: Foreign Domain A Memory Access from Domain B EL0
 * Tries to read 0x41000000. Hardware must raise Translation Fault (EC=0x24).
 */
.global svc_test_foreign_mem_b_to_a
.type svc_test_foreign_mem_b_to_a, %function
svc_test_foreign_mem_b_to_a:
	ldr x1, =0x41000000
	ldr x0, [x1]
	mov x0, #0
	svc #0

/*
 * Shared Memory Step 1: Domain A EL0 writes magic value to 0x411c0000.
 */
.global svc_test_shared_write_a
.type svc_test_shared_write_a, %function
svc_test_shared_write_a:
	ldr x1, =0x411c0000
	ldr x2, =0xcafebabe00000001
	str x2, [x1]
	mov x0, #0
	svc #0

/*
 * Shared Memory Step 2: Domain B EL0 reads 0x411c0000, checks, writes response.
 */
.global svc_test_shared_read_modify_b
.type svc_test_shared_read_modify_b, %function
svc_test_shared_read_modify_b:
	ldr x1, =0x411c0000
	ldr x0, [x1]
	ldr x2, =0xcafebabe00000001
	cmp x0, x2
	b.ne 3f
	ldr x2, =0xfeedface00000002
	str x2, [x1]
	mov x0, #0
	svc #0
3:
	mov x0, #1
	svc #0

/*
 * Shared Memory Step 3: Domain A EL0 reads 0x411c0000 and verifies response.
 */
.global svc_test_shared_verify_a
.type svc_test_shared_verify_a, %function
svc_test_shared_verify_a:
	ldr x1, =0x411c0000
	ldr x0, [x1]
	ldr x2, =0xfeedface00000002
	cmp x0, x2
	b.ne 4f
	mov x0, #0
	svc #0
4:
	mov x0, #1
	svc #0

/*
 * Minimal EL0 synthetic baseline (just returns OK via SVC #0 immediately).
 */
.global svc_minimal_return_entry
.type svc_minimal_return_entry, %function
svc_minimal_return_entry:
	mov x0, #0
	svc #0

/*
 * Camera Service calling Storage Service via Cross-Layer Gate (SVC #2):
 */
.global svc_camera_cross_layer_call
.type svc_camera_cross_layer_call, %function
svc_camera_cross_layer_call:
	ldr x1, =0x41000000
	ldr x2, =0x100a0001100a0001
	str x2, [x1]

	ldr x4, =0x411c0010
	ldr x3, [x4]

	mov x0, #40
	mov x1, #1
	mov x2, #2
	svc #2

	cbnz x0, 6f

	ldr x1, =0x41000000
	ldr x2, =0x100a0001100a0001
	ldr x3, [x1]
	cmp x2, x3
	b.ne 6f

	mov x0, #0
	svc #0
6:
	mov x0, #1
	svc #0

/*
 * Camera Service calling Storage Service via Elevator Gate (SVC #3):
 */
.global svc_camera_elevator_call
.type svc_camera_elevator_call, %function
svc_camera_elevator_call:
	ldr x1, =0x41000000
	ldr x2, =0x100a0001100a0001
	str x2, [x1]

	ldr x4, =0x411c0018
	ldr x3, [x4]

	mov x0, #40
	mov x1, #1
	mov x2, #2
	svc #3

	cbnz x0, 7f

	ldr x1, =0x41000000
	ldr x2, =0x100a0001100a0001
	ldr x3, [x1]
	cmp x2, x3
	b.ne 7f

	mov x0, #0
	svc #0
7:
	mov x0, #1
	svc #0

/*
 * Camera Service calling Display with intentionally invalid/revoked capability:
 */
.global svc_camera_invalid_cap_call
.type svc_camera_invalid_cap_call, %function
svc_camera_invalid_cap_call:
	mov x0, #20
	mov x1, #1
	mov x2, #1
	ldr x3, =0xdeadbeef
	svc #1
	/* Return the error status returned in x0 to kernel */
	svc #0

/*
 * Preemptive Multitasking Test Tasks:
 * Each task accesses its own domain-private page to verify MMU isolation,
 * and updates its progress counter in the shared page (0x411c0000).
 */
.global task_counter_a
.type task_counter_a, %function
task_counter_a:
	ldr x0, =0x41000000
	ldr x1, =0x411c0050
1:
	ldr x3, [x0]
	ldr x2, [x1]
	add x2, x2, #1
	str x2, [x1]
	b 1b

.global task_counter_b
.type task_counter_b, %function
task_counter_b:
	ldr x0, =0x41100000
	ldr x1, =0x411c0058
1:
	ldr x3, [x0]
	ldr x2, [x1]
	add x2, x2, #1
	str x2, [x1]
	b 1b

.global task_counter_c
.type task_counter_c, %function
task_counter_c:
	ldr x0, =0x41180000
	ldr x1, =0x411c0060
1:
	ldr x3, [x0]
	ldr x2, [x1]
	add x2, x2, #1
	str x2, [x1]
	b 1b

/*
 * Task POSIX Runner: executes write, getpid, clock_gettime, and returns OK
 */
.global task_posix_runner
.type task_posix_runner, %function
task_posix_runner:
	/* 1. write(1, buf, 10) */
	mov x0, #1
	ldr x1, =0x411c0020
	mov x2, #10
	mov x8, #1 /* LETTUCE_SYS_WRITE */
	svc #5
	cmp x0, #10
	b.ne 9f

	/* 2. getpid() */
	mov x8, #4 /* LETTUCE_SYS_GETPID */
	svc #5
	cbz x0, 9f

	/* 3. clock_gettime(CLOCK_MONOTONIC, tp) */
	mov x0, #1 /* CLOCK_MONOTONIC */
	ldr x1, =0x411c0040
	mov x8, #5 /* LETTUCE_SYS_CLOCK_GETTIME */
	svc #5
	cbnz x0, 9f

	/* Success: return 0 via SVC #0 */
	mov x0, #0
	svc #0

9:
	mov x0, #1
	svc #0

/*
 * Syscall Benchmark: calls getpid() and clock_gettime() via SVC #5
 */
.global task_bench_syscall
.type task_bench_syscall, %function
task_bench_syscall:
	mov x8, #4 /* LETTUCE_SYS_GETPID */
	svc #5
	mov x0, #1 /* CLOCK_MONOTONIC */
	ldr x1, =0x411c0040
	mov x8, #5 /* LETTUCE_SYS_CLOCK_GETTIME */
	svc #5
	mov x0, #0
	svc #0
