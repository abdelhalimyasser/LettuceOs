/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CAPABILITY_INTERNAL_H
#define CAPABILITY_INTERNAL_H

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <lettuce/capability.h>

/*
 * Prototype limit.
 *
 * 4096 entries keeps the table small while being more than enough
 * for the research prototype.
 */
#define LETTUCE_CAPABILITY_TABLE_SIZE 4096u

typedef struct
{
    LettuceServiceId owner;
    LettuceServiceId target;
    LettuceResourceId resource;
    uint32_t operations;
    uint16_t generation;
    bool active;
} LettuceCapabilityEntry;


/*
 * Initializes the capability subsystem.
 */
void lettuce_capability_init(void);


/*
 * Creates a capability owned by `owner`.
 *
 * Returns LETTUCE_CAPABILITY_INVALID when no free slot exists.
 */
LettuceCapabilityHandle lettuce_capability_create(
    LettuceServiceId owner,
    LettuceServiceId target,
    uint32_t operations,
    LettuceResourceId resource
);


/*
 * Revokes an existing capability.
 *
 * Returns false when the handle is invalid or stale.
 */
bool lettuce_capability_revoke(
    LettuceCapabilityHandle handle
);


/*
 * Validates whether a capability authorizes an operation.
 *
 * IMPORTANT:
 * `caller` must eventually come from the kernel's current execution
 * context. A userspace service must never be allowed to choose its
 * own caller identity.
 */
bool lettuce_capability_check(
    LettuceCapabilityHandle handle,
    LettuceServiceId caller,
    LettuceServiceId target,
    LettuceCapabilityOperation operation,
    LettuceResourceId resource
);

#endif