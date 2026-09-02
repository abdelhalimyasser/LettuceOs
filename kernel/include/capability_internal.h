/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LETTUCE_CAPABILITY_INTERNAL_H
#define LETTUCE_CAPABILITY_INTERNAL_H

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <lettuce/capability.h>

#define LETTUCE_CAPABILITY_TABLE_SIZE 4096u

/*
 * Kernel-private representation of a single capability entry.
 *
 * Field ordering is tuned for compactness and minimal validation work:
 * - owner/target/resource/operations are the authorization fields
 * - generation catches stale handles
 * - active indicates whether the slot is presently allocated or revoked
 *
 * This is not a public type; services never write these fields directly.
 */
typedef struct LettuceCapabilityEntry
{
    LettuceServiceId owner;
    LettuceServiceId target;
    LettuceOperationId operation;
    LettuceResourceId resource;
    uint32_t permissions;
    uint16_t generation;
    bool active;
} LettuceCapabilityEntry;

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

_Static_assert(sizeof(LettuceCapabilityEntry) == 24,
              "LettuceCapabilityEntry should remain compact and 32-bit aligned.");

#endif
