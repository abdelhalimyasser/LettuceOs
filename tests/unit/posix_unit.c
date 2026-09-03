/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: tests/unit/posix_unit.c
 *
 * Purpose:
 *   Host-side unit tests for the POSIX-lite descriptor and syscall support.
 *
 * Success condition:
 *   Fixed descriptor operations and their error handling conform to the
 *   supported POSIX-lite contract.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "posix.h"
#include "fd.h"
#include "task.h"

int main(void)
{
	printf("Running posix_unit tests...\n");

	lettuce_fd_table_init();
	lettuce_task_init_subsystem();

	/* 1. write() valid stdout/stderr */
	const char msg[] = "POSIX test message\n";
	ssize_t written = lettuce_write(1, msg, strlen(msg));
	assert(written == (ssize_t)strlen(msg));

	written = lettuce_write(2, msg, strlen(msg));
	assert(written == (ssize_t)strlen(msg));

	/* Invalid fd */
	errno = 0;
	written = lettuce_write(99, msg, strlen(msg));
	assert(written == -1);
	assert(errno == EBADF);

	/* NULL buffer */
	errno = 0;
	written = lettuce_write(1, NULL, 10);
	assert(written == -1);
	assert(errno == EFAULT);

	/* Zero count */
	written = lettuce_write(1, msg, 0);
	assert(written == 0);

	/* 2. read() */
	char inbuf[16];
	ssize_t read_bytes = lettuce_read(0, inbuf, sizeof(inbuf));
	assert(read_bytes == 0); /* EOF in non-interactive mode */

	/* Invalid fd */
	errno = 0;
	read_bytes = lettuce_read(99, inbuf, sizeof(inbuf));
	assert(read_bytes == -1);
	assert(errno == EBADF);

	/* NULL buffer */
	errno = 0;
	read_bytes = lettuce_read(0, NULL, 5);
	assert(read_bytes == -1);
	assert(errno == EFAULT);

	/* 3. close() */
	int rc = lettuce_close(1);
	assert(rc == 0);

	errno = 0;
	rc = lettuce_close(99);
	assert(rc == -1);
	assert(errno == EBADF);

	/* 4. getpid() */
	pid_t pid = lettuce_getpid();
	assert(pid >= 1);

	LettuceTask *t = lettuce_task_create(10, 100, 0x1000, 0x2000, "Proc1");
	lettuce_task_set_current(t);
	pid = lettuce_getpid();
	assert(pid == (pid_t)t->id);

	/* 5. clock_gettime() */
	struct timespec ts;
	rc = lettuce_clock_gettime(CLOCK_MONOTONIC, &ts);
	assert(rc == 0);
	assert(ts.tv_sec >= 0);
	assert(ts.tv_nsec >= 0 && ts.tv_nsec < 1000000000);

	/* Invalid clock id */
	errno = 0;
	rc = lettuce_clock_gettime(999, &ts);
	assert(rc == -1);
	assert(errno == EINVAL);

	/* NULL tp */
	errno = 0;
	rc = lettuce_clock_gettime(CLOCK_MONOTONIC, NULL);
	assert(rc == -1);
	assert(errno == EFAULT);

	/* 6. nanosleep() */
	struct timespec req = {.tv_sec = 0, .tv_nsec = 1000};
	rc = lettuce_nanosleep(&req, NULL);
	assert(rc == 0);

	/* Invalid nsec */
	struct timespec bad_req = {.tv_sec = 0, .tv_nsec = 2000000000};
	errno = 0;
	rc = lettuce_nanosleep(&bad_req, NULL);
	assert(rc == -1);
	assert(errno == EINVAL);

	/* NULL req */
	errno = 0;
	rc = lettuce_nanosleep(NULL, NULL);
	assert(rc == -1);
	assert(errno == EFAULT);

	printf("[PASS] posix_unit tests passed!\n");
	return 0;
}
