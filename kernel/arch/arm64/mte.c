/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/arch/arm64/mte.c
 *
 * Purpose:
 *   Probes ARM Memory Tagging Extension support where the platform exposes it.
 *
 * Design:
 *   Feature availability is reported accurately; this file does not emulate
 *   MTE or claim active tagged-memory protection when unsupported.
 */

#include "features.h"
#include "../../include/arch.h"

bool lettuce_arch_has_mte(void)
{
	uint64_t val;
	__asm__ __volatile__("mrs %0, id_aa64pfr1_el1" : "=r"(val));
	const uint32_t mte = (uint32_t)((val >> 8u) & 0xfu);
	return (mte >= 1u);
}
