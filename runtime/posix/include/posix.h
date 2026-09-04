/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: runtime/posix/include/posix.h
 *
 * Purpose:
 *   Declares the POSIX-lite ABI shared by EL0 wrappers and EL1 syscall code.
 *
 * Flow:
 *   EL0 wrapper -> SVC #5 with x8 syscall identifier and argument registers
 *   -> EL1 validation and service -> ERET result.
 */

#ifndef LETTUCE_POSIX_H
#define LETTUCE_POSIX_H

#pragma once

#include <stddef.h>
#include <stdint.h>

typedef int pid_t;
typedef long ssize_t;
typedef int clockid_t;

#define CLOCK_MONOTONIC 1

#define EBADF   9
#define EINVAL  22
#define EFAULT  14
#define EINTR   4
#define ENOSYS  38

struct timespec {
	int64_t tv_sec;
	int64_t tv_nsec;
};

int *lettuce_errno_location(void);
#define errno (*lettuce_errno_location())

ssize_t lettuce_write(int fd, const void *buf, size_t count);
ssize_t lettuce_read(int fd, void *buf, size_t count);
int lettuce_close(int fd);
pid_t lettuce_getpid(void);
int lettuce_clock_gettime(clockid_t clk_id, struct timespec *tp);
int lettuce_nanosleep(const struct timespec *req, struct timespec *rem);

#ifndef LETTUCE_NO_POSIX_ALIASES
#define write lettuce_write
#define read lettuce_read
#define close lettuce_close
#define getpid lettuce_getpid
#define clock_gettime lettuce_clock_gettime
#define nanosleep lettuce_nanosleep
#endif

#endif /* LETTUCE_POSIX_H */
