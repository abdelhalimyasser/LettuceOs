/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: include/lettuce/service.h
 *
 * Purpose:
 *   Defines public service descriptors, classification layers, and the
 *   Same-Layer call contract.
 *
 * Design:
 *   Layers classify services rather than prescribe routing hops; registry and
 *   dispatch state remain supervisor-owned.
 */

#ifndef LETTUCE_SERVICE_H
#define LETTUCE_SERVICE_H

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <lettuce/capability.h>
#include <lettuce/errors.h>
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
 * The ABI is intentionally compact and intentionally includes the service's
 * logical protection domain, which the same-layer path uses before dispatching
 * a trusted entry point.
 */
typedef struct LettuceServiceDescriptor
{
    LettuceServiceId id;
    LettuceLayer layer;
    uint8_t reserved[3];
    LettuceDomainId domain;
    LettuceServiceFlags flags;
} LettuceServiceDescriptor;

LettuceStatus lettuce_same_layer_call(
    LettuceServiceId target_service_id,
    LettuceOperationId operation_id,
    LettuceResourceId resource_id,
    LettuceCapabilityHandle capability_handle);

_Static_assert(sizeof(LettuceLayer) == 1, "LettuceLayer must remain one byte.");
_Static_assert(sizeof(LettuceServiceFlags) == 4, "LettuceServiceFlags must be a 32-bit bitmask.");
_Static_assert(sizeof(LettuceServiceDescriptor) == 16,
              "LettuceServiceDescriptor must remain compact and ABI-stable at 16 bytes.");
_Static_assert(_Alignof(LettuceServiceDescriptor) == 4,
              "LettuceServiceDescriptor must retain 32-bit alignment.");
_Static_assert(offsetof(LettuceServiceDescriptor, id) == 0,
              "LettuceServiceDescriptor.id must remain at offset 0.");
_Static_assert(offsetof(LettuceServiceDescriptor, layer) == 4,
              "LettuceServiceDescriptor.layer must remain at offset 4.");
_Static_assert(offsetof(LettuceServiceDescriptor, domain) == 8,
              "LettuceServiceDescriptor.domain must remain at offset 8.");
_Static_assert(offsetof(LettuceServiceDescriptor, flags) == 12,
              "LettuceServiceDescriptor.flags must remain at offset 12.");

#endif
