/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: include/lettuce/abi.h
 *
 * Purpose:
 *   Defines version, width, and alignment constants for the public Lettuce
 *   ABI contract.
 *
 * Design:
 *   These are compile-time metadata values only; this header owns no runtime
 *   control flow or service state.
 */

#ifndef LETTUCE_ABI_H
#define LETTUCE_ABI_H

#pragma once

#include <stdint.h>

/*
 * ABI compatibility constants.
 *
 * These are compile-time metadata values only; this header does not define any
 * runtime logic or execution paths.
 */
#define LETTUCE_ABI_VERSION_MAJOR 1u
#define LETTUCE_ABI_VERSION_MINOR 0u
#define LETTUCE_ABI_VERSION_PATCH 0u
#define LETTUCE_ABI_VERSION ((uint32_t)((LETTUCE_ABI_VERSION_MAJOR << 24u) | \
                                      (LETTUCE_ABI_VERSION_MINOR << 16u) | \
                                      (LETTUCE_ABI_VERSION_PATCH)))

#define LETTUCE_ABI_COMPAT_MAJOR 1u
#define LETTUCE_ABI_COMPAT_MINOR 0u
#define LETTUCE_ABI_COMPAT_PATCH 0u
#define LETTUCE_ABI_COMPAT_VERSION ((uint32_t)((LETTUCE_ABI_COMPAT_MAJOR << 24u) | \
                                            (LETTUCE_ABI_COMPAT_MINOR << 16u) | \
                                            (LETTUCE_ABI_COMPAT_PATCH)))

#define LETTUCE_ABI_UINT32_WIDTH 32u
#define LETTUCE_ABI_STRUCT_ALIGNMENT 4u
#define LETTUCE_ABI_WORD_SIZE 4u

_Static_assert(LETTUCE_ABI_UINT32_WIDTH == 32u, "ABI width must match the 32-bit prototype.");
_Static_assert(LETTUCE_ABI_STRUCT_ALIGNMENT == 4u, "Structured ABI objects must align to 32-bit words.");

#endif
