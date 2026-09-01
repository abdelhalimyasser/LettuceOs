/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LETTUCE_SERVICE_H
#define LETTUCE_SERVICE_H

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <lettuce/types.h>

/*
 * Public service hierarchy level.
 */
typedef enum LettuceLayer : uint8_t
{
    LETTUCE_LAYER_MAIN = 0u,
    LETTUCE_LAYER_L1 = 1u,
    LETTUCE_LAYER_L2 = 2u,
    LETTUCE_LAYER_L3 = 3u,
    LETTUCE_LAYER_L4 = 4u
} LettuceLayer;

/*
 * Public service flags for lightweight runtime policy.
 */
typedef enum LettuceServiceFlags : uint32_t
{
    LETTUCE_SERVICE_FLAG_ACTIVE = 1u << 0,
    LETTUCE_SERVICE_FLAG_CRITICAL = 1u << 1,
    LETTUCE_SERVICE_FLAG_TRUSTED = 1u << 2,
    LETTUCE_SERVICE_FLAG_RESTARTABLE = 1u << 3
} LettuceServiceFlags;

/*
 * Compact public descriptor for a service instance.
 *
 * Layout intentionally matches a 32-bit ABI contract:
 *   - id:       4 bytes, offset 0
 *   - layer:    1 byte, offset 4
 *   - reserved: 3 bytes, offset 5..7 for packing to the next 32-bit boundary
 *   - flags:    4 bytes, offset 8
 *
 * Total size: 12 bytes. Alignment: 4 bytes.
 */
typedef struct LettuceServiceDescriptor
{
    LettuceServiceId id;
    LettuceLayer layer;
    uint8_t reserved[3];
    LettuceServiceFlags flags;
} LettuceServiceDescriptor;

_Static_assert(sizeof(LettuceLayer) == 1, "LettuceLayer must remain one byte.");
_Static_assert(sizeof(LettuceServiceFlags) == 4, "LettuceServiceFlags must be a 32-bit bitmask.");
_Static_assert(sizeof(LettuceServiceDescriptor) == 12,
              "LettuceServiceDescriptor must remain compact and ABI-stable at 12 bytes.");
_Static_assert(_Alignof(LettuceServiceDescriptor) == 4,
              "LettuceServiceDescriptor must retain 32-bit alignment.");
_Static_assert(offsetof(LettuceServiceDescriptor, id) == 0,
              "LettuceServiceDescriptor.id must remain at offset 0.");
_Static_assert(offsetof(LettuceServiceDescriptor, layer) == 4,
              "LettuceServiceDescriptor.layer must remain at offset 4.");
_Static_assert(offsetof(LettuceServiceDescriptor, flags) == 8,
              "LettuceServiceDescriptor.flags must remain at offset 8.");

#endif
