/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/arch/arm64/cpu.c
 *
 * Purpose:
 *   Implements ARM64 EL1 C handlers for supervisor calls, exceptions, and
 *   architecture-visible runtime services.
 *
 * Flow:
 *   EL0 exception entry -> decoded request and validated state -> kernel
 *   service or scheduler action -> assembly return path.
 *
 * Key invariants:
 *   User arguments are validated before privileged use.
 */

#include <stdint.h>

#include "../../include/arch.h"
#include "../include/kernel.h"
#include "gic.h"
#include "timer.h"
#include "irq.h"
#include "features.h"
#include "../include/elevator_asm.h"
#include "../include/task.h"
#include "../include/scheduler.h"
#include "../../runtime/posix/include/posix.h"

int64_t lettuce_kernel_sys_write(int fd, const void *buf, size_t count);
int64_t lettuce_kernel_sys_read(int fd, void *buf, size_t count);
int64_t lettuce_kernel_sys_close(int fd);
int64_t lettuce_kernel_sys_getpid(void);
int64_t lettuce_kernel_sys_clock_gettime(clockid_t clk_id, struct timespec *tp);
int64_t lettuce_kernel_sys_nanosleep(const struct timespec *req, struct timespec *rem, void *trap_frame);

#define LETTUCE_VIRT_UART_BASE UINT64_C(0x09000000)
#define LETTUCE_UART_DR 0x00u
#define LETTUCE_UART_FR 0x18u
#define LETTUCE_UART_FR_TXFF (1u << 5)

static volatile uint32_t *uart_register(uint32_t offset)
{
	return (volatile uint32_t *)(uintptr_t)(LETTUCE_VIRT_UART_BASE + offset);
}

void lettuce_arch_console_print_hex(uint64_t value)
{
	static const char digits[] = "0123456789abcdef";
	lettuce_arch_console_puts("0x");
	for (int shift = 60; shift >= 0; shift -= 4)
		lettuce_arch_console_putc(digits[(value >> (uint32_t)shift) & 0xfu]);
}

void lettuce_arch_console_print_dec(uint64_t value)
{
	char buf[24];
	int idx = 0;
	if (value == 0)
	{
		lettuce_arch_console_putc('0');
		return;
	}
	while (value > 0)
	{
		buf[idx++] = (char)('0' + (value % 10));
		value /= 10;
	}
	while (idx > 0)
	{
		lettuce_arch_console_putc(buf[--idx]);
	}
}

uint64_t lettuce_arch_current_el(void)
{
	uint64_t current_el;
	__asm__ __volatile__("mrs %0, CurrentEL" : "=r"(current_el));
	return (current_el >> 2u) & 0x3u;
}

void lettuce_arch_console_putc(char character)
{
	while ((*uart_register(LETTUCE_UART_FR) & LETTUCE_UART_FR_TXFF) != 0u)
	{
	}
	*uart_register(LETTUCE_UART_DR) = (uint32_t)(unsigned char)character;
}

void lettuce_arch_console_puts(const char *text)
{
	if (text == 0)
		return;
	while (*text != '\0')
	{
		if (*text == '\n')
			lettuce_arch_console_putc('\r');
		lettuce_arch_console_putc(*text++);
	}
}

void lettuce_arch_init(void)
{
	const uintptr_t vectors = (uintptr_t)lettuce_exception_vectors;
	const uint64_t current_el = lettuce_arch_current_el();
	if (current_el == 2u)
		__asm__ __volatile__("msr vbar_el2, %0" : : "r"(vectors) : "memory");
	else if (current_el == 1u)
		__asm__ __volatile__("msr vbar_el1, %0" : : "r"(vectors) : "memory");
	__asm__ __volatile__("isb" ::: "memory");
}

#define LETTUCE_ARCH_CONTEXT_MAX 8u

static LettuceArchContextFrame g_context_stack[LETTUCE_ARCH_CONTEXT_MAX];
static uint32_t g_context_stack_depth = 0;

static bool g_pac_enabled = false;
static volatile bool g_pac_expected_trap = false;
static volatile bool g_pac_trap_observed = false;

void lettuce_pac_enable(bool enable)
{
	g_pac_enabled = enable;
}

bool lettuce_pac_is_enabled(void)
{
	return g_pac_enabled;
}

void lettuce_pac_expect_trap(void)
{
	g_pac_expected_trap = true;
	g_pac_trap_observed = false;
}

bool lettuce_pac_trap_observed(void)
{
	return g_pac_trap_observed;
}

void lettuce_arch_context_corrupt_top_elr(uint64_t xor_mask)
{
	if (g_context_stack_depth > 0)
		g_context_stack[g_context_stack_depth - 1].elr ^= xor_mask;
}

static volatile uint32_t g_preempt_disable_count = 0;
static bool g_elevator_use_asm = true;

void lettuce_preempt_disable(void)
{
	g_preempt_disable_count++;
}

void lettuce_preempt_enable(void)
{
	if (g_preempt_disable_count > 0)
		g_preempt_disable_count--;
}

bool lettuce_preempt_is_enabled(void)
{
	return (g_preempt_disable_count == 0);
}

void lettuce_elevator_set_use_asm(bool use_asm)
{
	g_elevator_use_asm = use_asm;
}

bool lettuce_elevator_is_using_asm(void)
{
	return g_elevator_use_asm;
}

bool lettuce_arch_context_push(LettuceServiceId svc, LettuceDomainId dom, uint64_t elr, uint64_t spsr, uint64_t sp_el0)
{
	if (g_context_stack_depth >= LETTUCE_ARCH_CONTEXT_MAX)
		return false;

	uint64_t stored_elr = elr;
	if (g_pac_enabled)
		stored_elr = lettuce_pac_sign(elr, (uint64_t)svc);

	g_context_stack[g_context_stack_depth++] = (LettuceArchContextFrame){
		.service_id = svc,
		.domain_id = dom,
		.elr = stored_elr,
		.spsr = spsr,
		.sp_el0 = sp_el0
	};
	return true;
}

bool lettuce_arch_context_pop(LettuceArchContextFrame *out_frame)
{
	if (g_context_stack_depth == 0)
		return false;
	*out_frame = g_context_stack[--g_context_stack_depth];
	if (g_pac_enabled)
		out_frame->elr = lettuce_pac_auth(out_frame->elr, (uint64_t)out_frame->service_id);
	return true;
}

uint32_t lettuce_arch_context_depth(void)
{
	return g_context_stack_depth;
}

void lettuce_arch_sync_el1_handler(LettuceTrapFrame *frame)
{
	uint64_t esr;
	uint64_t far;
	if (lettuce_arch_current_el() == 2u)
	{
		__asm__ __volatile__("mrs %0, esr_el2" : "=r"(esr));
		__asm__ __volatile__("mrs %0, far_el2" : "=r"(far));
	}
	else
	{
		__asm__ __volatile__("mrs %0, esr_el1" : "=r"(esr));
		__asm__ __volatile__("mrs %0, far_el1" : "=r"(far));
	}
	const uint64_t exception_class = (esr >> 26u) & 0x3fu;
	const uint64_t fault_status = esr & 0x3fu;

	lettuce_arch_console_puts("[EXCEPTION] SyncEL1 ESR=");
	lettuce_arch_console_print_hex(esr);
	lettuce_arch_console_puts(" ELR=");
	lettuce_arch_console_print_hex(frame->elr);
	lettuce_arch_console_puts(" FAR=");
	lettuce_arch_console_print_hex(far);
	lettuce_arch_console_puts(" EC=");
	lettuce_arch_console_print_hex(exception_class);
	lettuce_arch_console_puts(" FSC=");
	lettuce_arch_console_print_hex(fault_status);

	if (exception_class == 0x20u || exception_class == 0x21u)
	{
		lettuce_arch_console_puts(" (instruction_abort)");
	}
	else if (exception_class == 0x24u || exception_class == 0x25u)
	{
		if (fault_status >= 0x4u && fault_status <= 0x7u)
			lettuce_arch_console_puts(" (translation_fault)");
		else if (fault_status >= 0x8u && fault_status <= 0xbu)
			lettuce_arch_console_puts(" (access_flag_fault)");
		else if (fault_status >= 0xcu && fault_status <= 0xfu)
			lettuce_arch_console_puts(" (permission_fault)");
		else
			lettuce_arch_console_puts(" (data_abort)");
	}
	else
	{
		lettuce_arch_console_puts(" (other)");
	}

	lettuce_arch_console_puts(" domain=");
	lettuce_arch_console_print_hex(lettuce_mmu_current_domain());
	lettuce_arch_console_puts(" service=");
	lettuce_arch_console_print_hex(current_service_id());
	lettuce_arch_console_puts("\n");

	if ((exception_class == 0x24u || exception_class == 0x25u) &&
		lettuce_mmu_handle_fault(esr, far, frame->elr))
	{
		/* Expected fault verified: advance ELR past the faulting instruction */
		frame->elr += 4u;
		return;
	}

	if (exception_class == 0x1cu)
	{
		lettuce_arch_console_puts("[EXCEPTION] SyncEL1 PAC Trap ESR=");
		lettuce_arch_console_print_hex(esr);
		lettuce_arch_console_puts(" ELR=");
		lettuce_arch_console_print_hex(frame->elr);
		lettuce_arch_console_puts("\n");
		if (g_pac_expected_trap)
		{
			g_pac_trap_observed = true;
			g_pac_expected_trap = false;
			frame->elr += 4u;
			return;
		}
	}

	lettuce_arch_console_puts("unexpected EL1 exception - system halted\n");
	for (;;)
		__asm__ __volatile__("wfe");
}

void lettuce_arch_sync_lower_el_handler(LettuceTrapFrame *frame)
{
	uint64_t esr;
	uint64_t far;
	__asm__ __volatile__("mrs %0, esr_el1" : "=r"(esr));
	__asm__ __volatile__("mrs %0, far_el1" : "=r"(far));

	const uint64_t exception_class = (esr >> 26u) & 0x3fu;
	const uint64_t fault_status = esr & 0x3fu;

	/* SVC from AArch64 EL0 */
	if (exception_class == 0x15u)
	{
		const uint32_t imm = (uint32_t)(esr & 0xffffu);
		if (imm == LETTUCE_SVC_RETURN)
		{
			/* Service is returning status in x0 */
			if (g_context_stack_depth > 0)
			{
				/* Resuming caller EL0 service from nested call */
				LettuceArchContextFrame caller;
				lettuce_arch_context_pop(&caller);
				lettuce_mmu_leave(caller.domain_id);
				kernel_set_current_service_id(caller.service_id);

				const uint64_t return_status = frame->x[0];
				frame->elr = caller.elr;
				frame->spsr = caller.spsr;
				frame->sp_el0 = caller.sp_el0;
				frame->x[0] = return_status;
				return;
			}
			else
			{
				/* Top-level EL0 execution finished: resume kernel test harness */
				lettuce_el0_resume_kernel((int64_t)frame->x[0]);
				return;
			}
		}
		else if (imm == LETTUCE_SVC_CALL || imm == LETTUCE_SVC_CROSS_LAYER || imm == LETTUCE_SVC_ELEVATOR)
		{
			/*
			 * Protected call from EL0:
			 * imm 0x01: Same-Layer call
			 * imm 0x02: Cross-Layer call
			 * imm 0x03: Elevator call
			 *
			 * x0 = target_service_id
			 * x1 = operation_id
			 * x2 = resource_id
			 * x3 = capability_handle
			 *
			 * CRITICAL SECURITY PRINCIPLE:
			 * Caller identity is derived authoritatively from kernel state (current_service_id),
			 * NOT trusted from EL0!
			 */
			const LettuceServiceId target_svc = (LettuceServiceId)frame->x[0];
			const LettuceOperationId op_id = (LettuceOperationId)frame->x[1];
			const LettuceResourceId res_id = (LettuceResourceId)frame->x[2];
			const LettuceCapabilityHandle cap = (LettuceCapabilityHandle)frame->x[3];

			const LettuceServiceId caller_svc = current_service_id();
			const LettuceDomainId caller_dom = lettuce_mmu_current_domain();

			LettuceCallResolution resolution;
			LettuceStatus val_status;

			if (imm == LETTUCE_SVC_CALL)
			{
				val_status = lettuce_same_layer_validate(
					target_svc, op_id, res_id, cap, &resolution);
			}
			else if (imm == LETTUCE_SVC_CROSS_LAYER)
			{
				const LettuceCallMessage msg = {
					.target_service_id = target_svc,
					.operation_id = op_id,
					.resource_id = res_id,
					.capability_handle = cap
				};
				val_status = lettuce_cross_layer_validate(&msg, &resolution);
			}
			else /* LETTUCE_SVC_ELEVATOR */
			{
				const LettuceCallMessage msg = {
					.target_service_id = target_svc,
					.operation_id = op_id,
					.resource_id = res_id,
					.capability_handle = cap
				};
				val_status = lettuce_elevator_policy(&msg, &resolution);
			}

			if (val_status != LETTUCE_STATUS_OK)
			{
				/* Validation failed: return error directly to caller at EL0 */
				frame->x[0] = val_status;
				return;
			}

			if (resolution.entry == NULL || resolution.entry->entry == NULL)
			{
				frame->x[0] = LETTUCE_STATUS_INVALID_TARGET_ENTRY;
				return;
			}

			if (!lettuce_arch_context_push(caller_svc, caller_dom, frame->elr, frame->spsr, frame->sp_el0))
			{
				frame->x[0] = LETTUCE_STATUS_OVERFLOW;
				return;
			}

			if (imm == LETTUCE_SVC_ELEVATOR && g_elevator_use_asm)
			{
				const LettuceElevatorDescriptor desc = {
					.target_entry_pc = (uintptr_t)resolution.entry->entry,
					.target_sp_el0 = lettuce_arch_domain_stack_top(resolution.target->domain),
					.target_ttbr0_val = lettuce_arch_domain_ttbr0_val(resolution.target->domain),
					.caller_ttbr0_val = lettuce_arch_domain_ttbr0_val(caller_dom),
					.target_service = resolution.target->id,
					.target_domain = resolution.target->domain,
					.caller_service = caller_svc,
					.caller_domain = caller_dom
				};
				kernel_set_current_service_id(resolution.target->id);
				lettuce_mmu_set_current_domain(resolution.target->domain);
				lettuce_elevator_asm_transition(&desc, frame);
				return;
			}

			/* Perform hardware MMU domain switch and update authoritative service identity */
			lettuce_mmu_enter(resolution.target->domain);
			kernel_set_current_service_id(resolution.target->id);

			/* Prepare target EL0 execution frame */
			frame->elr = (uintptr_t)resolution.entry->entry;
			frame->sp_el0 = lettuce_arch_domain_stack_top(resolution.target->domain);
			frame->spsr = 0u; /* EL0t */
			return;
		}
		else if (imm == LETTUCE_SVC_YIELD)
		{
			lettuce_scheduler_yield(frame);
			return;
		}
		else if (imm == LETTUCE_SVC_SYSCALL)
		{
			const uint64_t sys_no = frame->x[8];
			switch (sys_no)
			{
			case LETTUCE_SYS_WRITE:
				frame->x[0] = (uint64_t)lettuce_kernel_sys_write((int)frame->x[0], (const void *)frame->x[1], (size_t)frame->x[2]);
				break;
			case LETTUCE_SYS_READ:
				frame->x[0] = (uint64_t)lettuce_kernel_sys_read((int)frame->x[0], (void *)frame->x[1], (size_t)frame->x[2]);
				break;
			case LETTUCE_SYS_CLOSE:
				frame->x[0] = (uint64_t)lettuce_kernel_sys_close((int)frame->x[0]);
				break;
			case LETTUCE_SYS_GETPID:
				frame->x[0] = (uint64_t)lettuce_kernel_sys_getpid();
				break;
			case LETTUCE_SYS_CLOCK_GETTIME:
				frame->x[0] = (uint64_t)lettuce_kernel_sys_clock_gettime((clockid_t)frame->x[0], (struct timespec *)frame->x[1]);
				break;
			case LETTUCE_SYS_NANOSLEEP:
				frame->x[0] = (uint64_t)lettuce_kernel_sys_nanosleep((const struct timespec *)frame->x[0], (struct timespec *)frame->x[1], frame);
				break;
			default:
				frame->x[0] = (uint64_t)(-ENOSYS);
				break;
			}
			return;
		}
		else
		{
			lettuce_arch_console_puts("[EXCEPTION] Unexpected SVC immediate: ");
			lettuce_arch_console_print_hex(imm);
			lettuce_arch_console_puts("\n");
			for (;;)
				__asm__ __volatile__("wfe");
		}
	}

	/* Data Abort from EL0 */
	if (exception_class == 0x24u || exception_class == 0x25u)
	{
		lettuce_arch_console_puts("[EXCEPTION] LowerEL DataAbort ESR=");
		lettuce_arch_console_print_hex(esr);
		lettuce_arch_console_puts(" ELR=");
		lettuce_arch_console_print_hex(frame->elr);
		lettuce_arch_console_puts(" FAR=");
		lettuce_arch_console_print_hex(far);
		lettuce_arch_console_puts(" EC=");
		lettuce_arch_console_print_hex(exception_class);
		lettuce_arch_console_puts(" FSC=");
		lettuce_arch_console_print_hex(fault_status);

		if (fault_status >= 0x4u && fault_status <= 0x7u)
			lettuce_arch_console_puts(" (translation_fault)");
		else if (fault_status >= 0x8u && fault_status <= 0xbu)
			lettuce_arch_console_puts(" (access_flag_fault)");
		else if (fault_status >= 0xcu && fault_status <= 0xfu)
			lettuce_arch_console_puts(" (permission_fault)");
		else
			lettuce_arch_console_puts(" (data_abort)");

		lettuce_arch_console_puts(" domain=");
		lettuce_arch_console_print_hex(lettuce_mmu_current_domain());
		lettuce_arch_console_puts(" service=");
		lettuce_arch_console_print_hex(current_service_id());
		lettuce_arch_console_puts("\n");

		if (lettuce_mmu_handle_fault(esr, far, frame->elr))
		{
			/* Expected fault verified: recover safely to EL1 continuation */
			lettuce_el0_resume_kernel(LETTUCE_STATUS_ABORTED);
			return;
		}

		lettuce_arch_console_puts("unexpected LowerEL DataAbort - halted\n");
		for (;;)
			__asm__ __volatile__("wfe");
	}

	/* Trapped MSR, MRS, System instruction, or Undefined privileged register access from EL0 */
	if (exception_class == 0x18u || exception_class == 0x00u)
	{
		lettuce_arch_console_puts("[EXCEPTION] LowerEL SysRegTrap ESR=");
		lettuce_arch_console_print_hex(esr);
		lettuce_arch_console_puts(" ELR=");
		lettuce_arch_console_print_hex(frame->elr);
		lettuce_arch_console_puts(" EC=");
		lettuce_arch_console_print_hex(exception_class);
		lettuce_arch_console_puts(" (trapped_sysreg/undefined) domain=");
		lettuce_arch_console_print_hex(lettuce_mmu_current_domain());
		lettuce_arch_console_puts(" service=");
		lettuce_arch_console_print_hex(current_service_id());
		lettuce_arch_console_puts("\n");

		if (lettuce_arch_handle_sysreg_trap(esr, frame->elr))
		{
			/* Expected sysreg trap verified: recover safely to EL1 continuation */
			lettuce_el0_resume_kernel(LETTUCE_STATUS_DENIED);
			return;
		}

		lettuce_arch_console_puts("unexpected LowerEL SysRegTrap - halted\n");
		for (;;)
			__asm__ __volatile__("wfe");
	}

	/* Instruction Abort from EL0 */
	if (exception_class == 0x20u || exception_class == 0x21u)
	{
		lettuce_arch_console_puts("[EXCEPTION] LowerEL InstructionAbort ESR=");
		lettuce_arch_console_print_hex(esr);
		lettuce_arch_console_puts(" ELR=");
		lettuce_arch_console_print_hex(frame->elr);
		lettuce_arch_console_puts(" FAR=");
		lettuce_arch_console_print_hex(far);
		lettuce_arch_console_puts("\n");
		for (;;)
			__asm__ __volatile__("wfe");
	}

	lettuce_arch_console_puts("[EXCEPTION] LowerEL Unknown exception class=");
	lettuce_arch_console_print_hex(exception_class);
	lettuce_arch_console_puts("\n");
	for (;;)
		__asm__ __volatile__("wfe");
}

void lettuce_arch_exception_unexpected(void)
{
	lettuce_arch_console_puts("[EXCEPTION] Unexpected exception vector hit - system halted\n");
	for (;;)
		__asm__ __volatile__("wfe");
}

void lettuce_arch_irq_el1_handler(LettuceTrapFrame *frame)
{
	(void)frame;
	const uint32_t intid = lettuce_gic_acknowledge();
	if (intid == GIC_INTID_VTIMER)
	{
		lettuce_timer_irq_handler();
	}
	if (intid != GIC_SPURIOUS_INTID)
	{
		lettuce_gic_end_of_interrupt(intid);
	}
}

void lettuce_arch_irq_lower_el_handler(LettuceTrapFrame *frame)
{
	const uint32_t intid = lettuce_gic_acknowledge();
	if (intid == GIC_INTID_VTIMER)
	{
		lettuce_timer_irq_handler();
		if (lettuce_scheduler_is_active() && lettuce_preempt_is_enabled())
		{
			lettuce_scheduler_tick(frame);
		}
	}
	if (intid != GIC_SPURIOUS_INTID)
	{
		lettuce_gic_end_of_interrupt(intid);
	}
}
