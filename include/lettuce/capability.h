/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LETTUCE_CAPABILITY_H
#define LETTUCE_CAPABILITY_H

#pragma once

#include <stdint.h>

#include <lettuce/types.h>

/*
 * Services receive only an opaque compact capability handle.
 * Capability metadata is kernel-owned and intentionally not writable by
 * service code.
 */
typedef uint32_t LettuceCapabilityHandle;

#define LETTUCE_CAPABILITY_INVALID ((LettuceCapabilityHandle)0u)

typedef enum LettuceCapabilityOperation : uint32_t
{
    LETTUCE_CAPABILITY_OP_NONE = 0u,
    LETTUCE_CAPABILITY_OP_CALL = 1u << 0,
    LETTUCE_CAPABILITY_OP_READ = 1u << 1,
    LETTUCE_CAPABILITY_OP_WRITE = 1u << 2,
    LETTUCE_CAPABILITY_OP_MAP = 1u << 3,
    LETTUCE_CAPABILITY_OP_SIGNAL = 1u << 4,
    LETTUCE_CAPABILITY_OP_CRITICAL = 1u << 5
} LettuceCapabilityOperation;

#define LETTUCE_CAP_CALL LETTUCE_CAPABILITY_OP_CALL
#define LETTUCE_CAP_READ LETTUCE_CAPABILITY_OP_READ
#define LETTUCE_CAP_WRITE LETTUCE_CAPABILITY_OP_WRITE
#define LETTUCE_CAP_MAP LETTUCE_CAPABILITY_OP_MAP
#define LETTUCE_CAP_SIGNAL LETTUCE_CAPABILITY_OP_SIGNAL
#define LETTUCE_CAP_CRITICAL LETTUCE_CAPABILITY_OP_CRITICAL
#define LETTUCE_CAP_NONE LETTUCE_CAPABILITY_OP_NONE

_Static_assert(sizeof(LettuceCapabilityHandle) == 4,
              "LettuceCapabilityHandle must be a compact 32-bit ABI handle.");

#endif
