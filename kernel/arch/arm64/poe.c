/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/arch/arm64/poe.c
 *
 * Purpose:
 *   Probes ARM Permission Overlay Extension support where the platform exposes
 *   it.
 *
 * Design:
 *   This file reports unsupported POE accurately and does not provide a
 *   software substitute under the architectural feature name.
 */

#include "features.h"
#include "../../include/arch.h"

bool lettuce_arch_has_poe(void)
{
	uint64_t val;
	__asm__ __volatile__("mrs %0, id_aa64mmfr3_el1" : "=r"(val));
	const uint32_t s1poe = (uint32_t)((val >> 36u) & 0xfu);
	return (s1poe >= 1u);
}

bool lettuce_arch_has_pac(void)
{
	uint64_t val;
	__asm__ __volatile__("mrs %0, id_aa64isar1_el1" : "=r"(val));
	const uint32_t apa = (uint32_t)((val >> 4u) & 0xfu);
	const uint32_t api = (uint32_t)((val >> 8u) & 0xfu);
	return (apa >= 1u || api >= 1u);
}

void lettuce_arch_features_probe_and_print(void)
{
	lettuce_arch_console_puts("Architecture Feature Status:\n");
	lettuce_arch_console_puts("  PAC: ");
	if (lettuce_arch_has_pac())
		lettuce_arch_console_puts("YES (active continuation signing)\n");
	else
		lettuce_arch_console_puts("NO\n");

	lettuce_arch_console_puts("  MTE: ");
	if (lettuce_arch_has_mte())
		lettuce_arch_console_puts("YES (instructions present; unbacked tag RAM in default virt; optional)\n");
	else
		lettuce_arch_console_puts("NO (operating normally without MTE)\n");

	lettuce_arch_console_puts("  POE: ");
	if (lettuce_arch_has_poe())
		lettuce_arch_console_puts("YES\n");
	else
		lettuce_arch_console_puts("NO (fallback to optimized MMU/ASID backend)\n");
}
