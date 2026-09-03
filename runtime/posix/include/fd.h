/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: runtime/posix/include/fd.h
 *
 * Purpose:
 *   Defines the fixed-size POSIX-lite descriptor abstraction and descriptor
 *   operation interface.
 *
 * Design:
 *   The public contract is statically bounded and does not own descriptor-table
 *   storage or privileged syscall dispatch.
 */

#ifndef LETTUCE_FD_H
#define LETTUCE_FD_H

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "posix.h"

#define LETTUCE_MAX_FDS 16u

typedef enum LettuceFdType {
	LETTUCE_FD_TYPE_UNUSED = 0,
	LETTUCE_FD_TYPE_CONSOLE_IN,
	LETTUCE_FD_TYPE_CONSOLE_OUT,
	LETTUCE_FD_TYPE_CONSOLE_ERR,
	LETTUCE_FD_TYPE_PIPE,
	LETTUCE_FD_TYPE_DEVICE
} LettuceFdType;

typedef struct LettuceFdOps {
	ssize_t (*read)(int fd, void *buf, size_t count);
	ssize_t (*write)(int fd, const void *buf, size_t count);
	int (*close)(int fd);
} LettuceFdOps;

typedef struct LettuceFileDescriptor {
	LettuceFdType type;
	uint32_t flags;
	const LettuceFdOps *ops;
	void *private_data;
} LettuceFileDescriptor;

void lettuce_fd_table_init(void);
LettuceFileDescriptor *lettuce_fd_get(int fd);
int lettuce_fd_install(int fd, LettuceFdType type, const LettuceFdOps *ops, void *private_data);

#endif /* LETTUCE_FD_H */
