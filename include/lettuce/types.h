/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: include/lettuce/types.h
 *
 * Purpose:
 *   Defines fixed-width public identifiers for services, resources, domains,
 *   and operations.
 *
 * Design:
 *   All identifiers remain 32-bit to keep ABI objects compact and independent
 *   of host pointer width.
 */

#ifndef LETTUCE_TYPES_H
#define LETTUCE_TYPES_H

#pragma once

#include <stdint.h>

/*
 * Public identity fields for the prototype ABI.
 *
 * All are intentionally 32-bit values so they remain compact and stable across
 * the kernel/runtime boundary without introducing pointer-sized or platform-
 * dependent data types.
 */
typedef uint32_t LettuceServiceId;
typedef uint32_t LettuceResourceId;
typedef uint32_t LettuceDomainId;
typedef uint32_t LettuceOperationId;

#define LETTUCE_SERVICE_ID_INVALID UINT32_C(0)
#define LETTUCE_RESOURCE_ID_INVALID UINT32_C(0)
#define LETTUCE_DOMAIN_ID_INVALID UINT32_C(0)
#define LETTUCE_OPERATION_ID_INVALID UINT32_C(0)

_Static_assert(sizeof(LettuceServiceId) == 4, "LettuceServiceId must be 32 bits.");
_Static_assert(sizeof(LettuceResourceId) == 4, "LettuceResourceId must be 32 bits.");
_Static_assert(sizeof(LettuceDomainId) == 4, "LettuceDomainId must be 32 bits.");
_Static_assert(sizeof(LettuceOperationId) == 4, "LettuceOperationId must be 32 bits.");

#endif
