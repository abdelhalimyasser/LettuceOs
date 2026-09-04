/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: runtime/posix/src/sys.c
 *
 * Purpose:
 *   Implements EL1-side POSIX-lite syscall services for descriptors, clock,
 *   sleep, and UART-facing operations.
 *
 * Key invariants:
 *   User pointers are validated and supervisor memory is rejected before use.
 */

#define LETTUCE_NO_POSIX_ALIASES
#include "../include/posix.h"
#include "../include/fd.h"
#include "../../../kernel/include/task.h"
#include "../../../kernel/include/scheduler.h"

#if defined(__aarch64__) && !__STDC_HOSTED__
#include "../../../kernel/include/arch.h"
#include "../../../kernel/arch/arm64/timer.h"
#define KERNEL_MEM_START UINT64_C(0x40000000)
#define KERNEL_MEM_END   UINT64_C(0x40200000)
#else
#include <time.h>
#endif

int64_t lettuce_kernel_sys_write(int fd, const void *buf, size_t count)
{
	if (count == 0)
		return 0;
	if (buf == NULL)
		return -EFAULT;

#if defined(__aarch64__) && !__STDC_HOSTED__
	const uintptr_t ptr = (uintptr_t)buf;
	if (ptr >= KERNEL_MEM_START && ptr < KERNEL_MEM_END)
		return -EFAULT;
#endif

	LettuceFileDescriptor *desc = lettuce_fd_get(fd);
	if (desc == NULL || desc->ops == NULL || desc->ops->write == NULL)
		return -EBADF;

	return (int64_t)desc->ops->write(fd, buf, count);
}

int64_t lettuce_kernel_sys_read(int fd, void *buf, size_t count)
{
	if (count == 0)
		return 0;
	if (buf == NULL)
		return -EFAULT;

#if defined(__aarch64__) && !__STDC_HOSTED__
	const uintptr_t ptr = (uintptr_t)buf;
	if (ptr >= KERNEL_MEM_START && ptr < KERNEL_MEM_END)
		return -EFAULT;
#endif

	LettuceFileDescriptor *desc = lettuce_fd_get(fd);
	if (desc == NULL || desc->ops == NULL || desc->ops->read == NULL)
		return -EBADF;

	return (int64_t)desc->ops->read(fd, buf, count);
}

int64_t lettuce_kernel_sys_close(int fd)
{
	LettuceFileDescriptor *desc = lettuce_fd_get(fd);
	if (desc == NULL || desc->ops == NULL || desc->ops->close == NULL)
		return -EBADF;

	return (int64_t)desc->ops->close(fd);
}

int64_t lettuce_kernel_sys_getpid(void)
{
	LettuceTask *curr = lettuce_task_current();
	if (curr != NULL)
		return (int64_t)curr->id;
	return 1;
}

int64_t lettuce_kernel_sys_clock_gettime(clockid_t clk_id, struct timespec *tp)
{
	if (clk_id != CLOCK_MONOTONIC)
		return -EINVAL;
	if (tp == NULL)
		return -EFAULT;

#if defined(__aarch64__) && !__STDC_HOSTED__
	const uintptr_t ptr = (uintptr_t)tp;
	if (ptr >= KERNEL_MEM_START && ptr < KERNEL_MEM_END)
		return -EFAULT;

	const uint64_t ticks = lettuce_arch_counter_read();
	const uint64_t freq = lettuce_arch_counter_frequency();
	if (freq == 0)
		return -EINVAL;

	tp->tv_sec = (int64_t)(ticks / freq);
	tp->tv_nsec = (int64_t)(((ticks % freq) * 1000000000ULL) / freq);
	return 0;
#else
	struct timespec host_tp;
	if (clock_gettime(CLOCK_MONOTONIC, &host_tp) != 0)
		return -EINVAL;
	tp->tv_sec = host_tp.tv_sec;
	tp->tv_nsec = host_tp.tv_nsec;
	return 0;
#endif
}

int64_t lettuce_kernel_sys_nanosleep(const struct timespec *req, struct timespec *rem, void *trap_frame)
{
	(void)rem;
	if (req == NULL)
		return -EFAULT;
	if (req->tv_nsec < 0 || req->tv_nsec >= 1000000000LL)
		return -EINVAL;

#if defined(__aarch64__) && !__STDC_HOSTED__
	const uintptr_t ptr = (uintptr_t)req;
	if (ptr >= KERNEL_MEM_START && ptr < KERNEL_MEM_END)
		return -EFAULT;

	const uint64_t freq = lettuce_arch_counter_frequency();
	const uint64_t delta_ticks = ((uint64_t)req->tv_sec * freq) +
	                             (((uint64_t)req->tv_nsec * freq) / 1000000000ULL);

	lettuce_scheduler_sleep_current(delta_ticks, trap_frame);
	return 0;
#else
	(void)trap_frame;
	return 0;
#endif
}
