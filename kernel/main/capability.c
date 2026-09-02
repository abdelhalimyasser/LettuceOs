/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../include/capability_internal.h"
#include "../include/kernel.h"
#include <lettuce/capability.h>

/*
 * Handle layout:
 *
 * 31           16 15            0
 * +---------------+--------------+
 * | generation    | slot + 1     |
 * +---------------+--------------+
 *
 * The low 16 bits encode the slot index + 1 so that handle 0 remains invalid.
 */
#define HANDLE_SLOT_MASK 0xFFFFu
#define HANDLE_GENERATION_SHIFT 16u
#define LETTUCE_CAPABILITY_KNOWN_PERMISSIONS \
    (LETTUCE_CAP_CALL | LETTUCE_CAP_READ | LETTUCE_CAP_WRITE | LETTUCE_CAP_MAP | \
     LETTUCE_CAP_SIGNAL | LETTUCE_CAP_CRITICAL)

static LettuceCapabilityEntry capability_table[LETTUCE_CAPABILITY_TABLE_SIZE];

static LettuceCapabilityHandle make_handle(uint16_t slot, uint16_t generation)
{
    return ((LettuceCapabilityHandle)generation << HANDLE_GENERATION_SHIFT) |
           ((LettuceCapabilityHandle)slot + 1u);
}

static bool decode_handle(LettuceCapabilityHandle handle, uint16_t *slot, uint16_t *generation)
{
    if (handle == LETTUCE_CAPABILITY_INVALID)
        return false;

    const uint32_t encoded_slot = handle & HANDLE_SLOT_MASK;
    if (encoded_slot == 0u)
        return false;

    const uint32_t decoded_slot = encoded_slot - 1u;
    if (decoded_slot >= LETTUCE_CAPABILITY_TABLE_SIZE)
        return false;

    *slot = (uint16_t)decoded_slot;
    *generation = (uint16_t)(handle >> HANDLE_GENERATION_SHIFT);
    return true;
}

void lettuce_capability_init(void)
{
    for (uint32_t i = 0; i < LETTUCE_CAPABILITY_TABLE_SIZE; ++i)
    {
        capability_table[i].owner = LETTUCE_SERVICE_ID_INVALID;
        capability_table[i].target = LETTUCE_SERVICE_ID_INVALID;
        capability_table[i].operation = LETTUCE_OPERATION_ID_INVALID;
        capability_table[i].resource = LETTUCE_RESOURCE_ID_INVALID;
        capability_table[i].permissions = LETTUCE_CAPABILITY_OP_NONE;
        capability_table[i].generation = 1u;
        capability_table[i].active = false;
    }
}

LettuceCapabilityHandle lettuce_capability_create(
    LettuceServiceId owner,
    LettuceServiceId target,
    LettuceOperationId operation,
    LettuceCapabilityOperation permissions,
    LettuceResourceId resource)
{
    if (owner == LETTUCE_SERVICE_ID_INVALID || target == LETTUCE_SERVICE_ID_INVALID ||
        operation == LETTUCE_OPERATION_ID_INVALID || resource == LETTUCE_RESOURCE_ID_INVALID)
        return LETTUCE_CAPABILITY_INVALID;

    if (permissions == LETTUCE_CAPABILITY_OP_NONE ||
        ((uint32_t)permissions & ~((uint32_t)LETTUCE_CAPABILITY_KNOWN_PERMISSIONS)) != 0u)
        return LETTUCE_CAPABILITY_INVALID;

    for (uint32_t i = 0; i < LETTUCE_CAPABILITY_TABLE_SIZE; ++i)
    {
        LettuceCapabilityEntry *entry = &capability_table[i];
        if (entry->active)
            continue;

        entry->owner = owner;
        entry->target = target;
        entry->operation = operation;
        entry->resource = resource;
        entry->permissions = permissions;
        entry->active = true;

        return make_handle((uint16_t)i, entry->generation);
    }

    return LETTUCE_CAPABILITY_INVALID;
}

bool lettuce_capability_revoke(LettuceCapabilityHandle handle)
{
    uint16_t slot = 0u;
    uint16_t generation = 0u;

    if (!decode_handle(handle, &slot, &generation))
        return false;

    LettuceCapabilityEntry *entry = &capability_table[slot];
    if (!entry->active || entry->generation != generation)
        return false;

    entry->owner = LETTUCE_SERVICE_ID_INVALID;
    entry->target = LETTUCE_SERVICE_ID_INVALID;
    entry->operation = LETTUCE_OPERATION_ID_INVALID;
    entry->resource = LETTUCE_RESOURCE_ID_INVALID;
    entry->permissions = LETTUCE_CAPABILITY_OP_NONE;
    entry->active = false;

    entry->generation += 1u;
    if (entry->generation == 0u)
        entry->generation = 1u;

    return true;
}

bool lettuce_capability_check(
    LettuceCapabilityHandle handle,
    LettuceServiceId target,
    LettuceOperationId operation,
    LettuceCapabilityOperation permission,
    LettuceResourceId resource)
{
    uint16_t slot = 0u;
    uint16_t generation = 0u;

    if (!decode_handle(handle, &slot, &generation))
        return false;

    const LettuceCapabilityEntry *entry = &capability_table[slot];
    if (!entry->active || entry->generation != generation)
        return false;

    const LettuceServiceId trusted_caller = current_service_id();
    if (trusted_caller == LETTUCE_SERVICE_ID_INVALID)
        return false;

    if (entry->owner != trusted_caller)
        return false;

    if (entry->target != target)
        return false;

    if (entry->operation != operation)
        return false;

    if (resource != entry->resource)
        return false;

    if ((entry->permissions & (uint32_t)permission) != (uint32_t)permission)
        return false;

    return true;
}
