/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: runtime/posix/src/fd.c
 *
 * Purpose:
 *   Implements the fixed-size POSIX-lite descriptor table and descriptor
 *   operations.
 *
 * Execution context:
 *   EL1 syscall-service support.
 *
 * Key invariants:
 *   Descriptor allocation is bounded and does not depend on dynamic storage.
 */

#define LETTUCE_NO_POSIX_ALIASES
#include "../include/fd.h"

#if defined(__aarch64__) && !__STDC_HOSTED__
#include "../../../kernel/include/arch.h"
#else
#include <stdio.h>
#endif

static LettuceFileDescriptor g_fd_table[LETTUCE_MAX_FDS];

static ssize_t console_read_op(int fd, void *buf, size_t count)
{
	(void)fd;
	if (buf == NULL && count > 0)
		return -EFAULT;
	if (count == 0)
		return 0;

	/*
	 * Minimal console input: in QEMU virt without interactive input,
	 * return 0 (EOF) or single byte if available.
	 */
	return 0;
}

static ssize_t console_write_op(int fd, const void *buf, size_t count)
{
	(void)fd;
	if (buf == NULL && count > 0)
		return -EFAULT;
	if (count == 0)
		return 0;

	const char *str = (const char *)buf;
	for (size_t i = 0; i < count; ++i)
	{
#if defined(__aarch64__) && !__STDC_HOSTED__
		lettuce_arch_console_putc(str[i]);
#else
		putchar(str[i]);
#endif
	}
	return (ssize_t)count;
}

static int console_close_op(int fd)
{
	(void)fd;
	/* Standard streams remain open */
	return 0;
}

static const LettuceFdOps g_console_in_ops = {
	.read = console_read_op,
	.write = NULL,
	.close = console_close_op
};

static const LettuceFdOps g_console_out_ops = {
	.read = NULL,
	.write = console_write_op,
	.close = console_close_op
};

void lettuce_fd_table_init(void)
{
	for (uint32_t i = 0; i < LETTUCE_MAX_FDS; ++i)
	{
		g_fd_table[i].type = LETTUCE_FD_TYPE_UNUSED;
		g_fd_table[i].flags = 0;
		g_fd_table[i].ops = NULL;
		g_fd_table[i].private_data = NULL;
	}

	/* fd 0: stdin */
	g_fd_table[0].type = LETTUCE_FD_TYPE_CONSOLE_IN;
	g_fd_table[0].ops = &g_console_in_ops;

	/* fd 1: stdout */
	g_fd_table[1].type = LETTUCE_FD_TYPE_CONSOLE_OUT;
	g_fd_table[1].ops = &g_console_out_ops;

	/* fd 2: stderr */
	g_fd_table[2].type = LETTUCE_FD_TYPE_CONSOLE_ERR;
	g_fd_table[2].ops = &g_console_out_ops;
}

LettuceFileDescriptor *lettuce_fd_get(int fd)
{
	if (fd < 0 || (uint32_t)fd >= LETTUCE_MAX_FDS)
		return NULL;
	if (g_fd_table[fd].type == LETTUCE_FD_TYPE_UNUSED)
		return NULL;
	return &g_fd_table[fd];
}

int lettuce_fd_install(int fd, LettuceFdType type, const LettuceFdOps *ops, void *private_data)
{
	if (fd < 0 || (uint32_t)fd >= LETTUCE_MAX_FDS)
		return -EBADF;
	g_fd_table[fd].type = type;
	g_fd_table[fd].flags = 0;
	g_fd_table[fd].ops = ops;
	g_fd_table[fd].private_data = private_data;
	return 0;
}
