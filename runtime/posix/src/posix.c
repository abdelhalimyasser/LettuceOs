/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: runtime/posix/src/posix.c
 *
 * Purpose:
 *   Implements EL0-facing POSIX-lite wrappers around the supervisor syscall
 *   ABI.
 *
 * Flow:
 *   C wrapper -> SVC #5 -> EL1 syscall dispatcher -> result returned to EL0.
 *
 * This file does not:
 *   Implement privileged file-descriptor or pointer-validation policy.
 */

#define LETTUCE_NO_POSIX_ALIASES
#include "../include/posix.h"

static int g_lettuce_errno = 0;

int *lettuce_errno_location(void)
{
	return &g_lettuce_errno;
}

#ifdef __aarch64__
#define LETTUCE_SVC_SYSCALL 0x05u

#define LETTUCE_SYS_WRITE         1u
#define LETTUCE_SYS_READ          2u
#define LETTUCE_SYS_CLOSE         3u
#define LETTUCE_SYS_GETPID        4u
#define LETTUCE_SYS_CLOCK_GETTIME 5u
#define LETTUCE_SYS_NANOSLEEP     6u

static inline int64_t lettuce_syscall0(uint64_t sys_no)
{
	register int64_t x0 __asm__("x0");
	register uint64_t x8 __asm__("x8") = sys_no;
	__asm__ __volatile__("svc #5" : "=r"(x0) : "r"(x8) : "memory");
	return x0;
}

static inline int64_t lettuce_syscall1(uint64_t sys_no, uint64_t arg0)
{
	register int64_t x0 __asm__("x0") = (int64_t)arg0;
	register uint64_t x8 __asm__("x8") = sys_no;
	__asm__ __volatile__("svc #5" : "+r"(x0) : "r"(x8) : "memory");
	return x0;
}

static inline int64_t lettuce_syscall2(uint64_t sys_no, uint64_t arg0, uint64_t arg1)
{
	register int64_t x0 __asm__("x0") = (int64_t)arg0;
	register uint64_t x1 __asm__("x1") = arg1;
	register uint64_t x8 __asm__("x8") = sys_no;
	__asm__ __volatile__("svc #5" : "+r"(x0) : "r"(x1), "r"(x8) : "memory");
	return x0;
}

static inline int64_t lettuce_syscall3(uint64_t sys_no, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
	register int64_t x0 __asm__("x0") = (int64_t)arg0;
	register uint64_t x1 __asm__("x1") = arg1;
	register uint64_t x2 __asm__("x2") = arg2;
	register uint64_t x8 __asm__("x8") = sys_no;
	__asm__ __volatile__("svc #5" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory");
	return x0;
}
#else
/* Host prototypes for kernel sys functions */
int64_t lettuce_kernel_sys_write(int fd, const void *buf, size_t count);
int64_t lettuce_kernel_sys_read(int fd, void *buf, size_t count);
int64_t lettuce_kernel_sys_close(int fd);
int64_t lettuce_kernel_sys_getpid(void);
int64_t lettuce_kernel_sys_clock_gettime(clockid_t clk_id, struct timespec *tp);
int64_t lettuce_kernel_sys_nanosleep(const struct timespec *req, struct timespec *rem, void *trap_frame);
#endif

ssize_t lettuce_write(int fd, const void *buf, size_t count)
{
#ifdef __aarch64__
	int64_t res = lettuce_syscall3(LETTUCE_SYS_WRITE, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
#else
	int64_t res = lettuce_kernel_sys_write(fd, buf, count);
#endif
	if (res < 0)
	{
		g_lettuce_errno = (int)(-res);
		return -1;
	}
	return (ssize_t)res;
}

ssize_t lettuce_read(int fd, void *buf, size_t count)
{
#ifdef __aarch64__
	int64_t res = lettuce_syscall3(LETTUCE_SYS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
#else
	int64_t res = lettuce_kernel_sys_read(fd, buf, count);
#endif
	if (res < 0)
	{
		g_lettuce_errno = (int)(-res);
		return -1;
	}
	return (ssize_t)res;
}

int lettuce_close(int fd)
{
#ifdef __aarch64__
	int64_t res = lettuce_syscall1(LETTUCE_SYS_CLOSE, (uint64_t)fd);
#else
	int64_t res = lettuce_kernel_sys_close(fd);
#endif
	if (res < 0)
	{
		g_lettuce_errno = (int)(-res);
		return -1;
	}
	return 0;
}

pid_t lettuce_getpid(void)
{
#ifdef __aarch64__
	int64_t res = lettuce_syscall0(LETTUCE_SYS_GETPID);
#else
	int64_t res = lettuce_kernel_sys_getpid();
#endif
	return (pid_t)res;
}

int lettuce_clock_gettime(clockid_t clk_id, struct timespec *tp)
{
#ifdef __aarch64__
	int64_t res = lettuce_syscall2(LETTUCE_SYS_CLOCK_GETTIME, (uint64_t)clk_id, (uint64_t)tp);
#else
	int64_t res = lettuce_kernel_sys_clock_gettime(clk_id, tp);
#endif
	if (res < 0)
	{
		g_lettuce_errno = (int)(-res);
		return -1;
	}
	return 0;
}

int lettuce_nanosleep(const struct timespec *req, struct timespec *rem)
{
#ifdef __aarch64__
	int64_t res = lettuce_syscall2(LETTUCE_SYS_NANOSLEEP, (uint64_t)req, (uint64_t)rem);
#else
	int64_t res = lettuce_kernel_sys_nanosleep(req, rem, NULL);
#endif
	if (res < 0)
	{
		g_lettuce_errno = (int)(-res);
		return -1;
	}
	return 0;
}
