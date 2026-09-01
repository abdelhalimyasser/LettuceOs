/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LETTUCE_ERRORS_H
#define LETTUCE_ERRORS_H

#pragma once

#include <stdint.h>

/*
 * Compact status values for kernel/runtime coordination.
 *
 * The ABI is intentionally simple: a single 32-bit integer status code with a
 * small set of common success/failure cases and no errno dependency.
 */
typedef enum LettuceStatus : uint32_t
{
    LETTUCE_STATUS_OK = 0u,
    LETTUCE_STATUS_ERROR = 1u,
    LETTUCE_STATUS_INVALID_ARGUMENT = 2u,
    LETTUCE_STATUS_DENIED = 3u,
    LETTUCE_STATUS_NOT_FOUND = 4u,
    LETTUCE_STATUS_BUSY = 5u,
    LETTUCE_STATUS_TIMEOUT = 6u,
    LETTUCE_STATUS_UNAVAILABLE = 7u,
    LETTUCE_STATUS_NOT_SUPPORTED = 8u,
    LETTUCE_STATUS_ABORTED = 9u,
    LETTUCE_STATUS_OVERFLOW = 10u,
    LETTUCE_STATUS_INTERNAL = 11u,
    LETTUCE_STATUS_INVALID_STATE = 12u
} LettuceStatus;

_Static_assert(sizeof(LettuceStatus) == 4, "LettuceStatus must remain a 32-bit ABI value.");

#endif
