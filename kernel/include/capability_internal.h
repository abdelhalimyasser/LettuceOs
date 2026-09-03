/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/include/capability_internal.h
 *
 * Purpose:
 *   Declares supervisor-owned capability-table operations and validation APIs.
 *
 * Key invariants:
 *   - Capability metadata remains outside the public service ABI.
 *   - Call paths validate requested target, operation, resource, and rights.
 */

#ifndef LETTUCE_CAPABILITY_INTERNAL_H
#define LETTUCE_CAPABILITY_INTERNAL_H

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <lettuce/capability.h>

#define LETTUCE_CAPABILITY_TABLE_SIZE 4096u

/*
 * Kernel-private representation of a single capability entry.
 *
 * Field ordering is tuned for locality and early-exit validation:
 * - generation and active are placed in the first 4-byte word to enable
 *   immediate rejection of stale/unallocated handles before touching other fields.
 * - owner and target follow immediately.
 * - operation, resource, and permissions complete the 24-byte structure.
 *
 * This is not a public type; services never write these fields directly.
 */
typedef struct LettuceCapabilityEntry
{
    uint16_t generation;          /* Offset  0: 2 bytes */
    bool active;                  /* Offset  2: 1 byte  */
    uint8_t reserved;             /* Offset  3: 1 byte explicit pad */
    LettuceServiceId owner;       /* Offset  4: 4 bytes */
    LettuceServiceId target;      /* Offset  8: 4 bytes */
    LettuceOperationId operation; /* Offset 12: 4 bytes */
    LettuceResourceId resource;   /* Offset 16: 4 bytes */
    uint32_t permissions;        /* Offset 20: 4 bytes */
} LettuceCapabilityEntry;

_Static_assert(sizeof(LettuceCapabilityEntry) == 24,
               "LettuceCapabilityEntry must remain compact and 32-bit aligned at exactly 24 bytes.");
_Static_assert(_Alignof(LettuceCapabilityEntry) == 4,
               "LettuceCapabilityEntry must retain 32-bit alignment.");
_Static_assert(offsetof(LettuceCapabilityEntry, generation) == 0,
               "generation must reside at offset 0 for rapid validation.");
_Static_assert(offsetof(LettuceCapabilityEntry, active) == 2,
               "active flag must reside at offset 2.");

void lettuce_capability_init(void);

LettuceCapabilityHandle lettuce_capability_create(
    LettuceServiceId owner,
    LettuceServiceId target,
    LettuceOperationId operation,
    LettuceCapabilityOperation permissions,
    LettuceResourceId resource
);

bool lettuce_capability_revoke(
    LettuceCapabilityHandle handle
);

bool lettuce_capability_check(
    LettuceCapabilityHandle handle,
    LettuceServiceId target,
    LettuceOperationId operation,
    LettuceCapabilityOperation permission,
    LettuceResourceId resource
);

#endif /* LETTUCE_CAPABILITY_INTERNAL_H */
