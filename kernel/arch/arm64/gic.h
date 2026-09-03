/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/arch/arm64/gic.h
 *
 * Purpose:
 *   Declares the GICv2 interface used by ARM64 interrupt and timer handling.
 *
 * Provides:
 *   Initialization, interrupt acknowledgement, and EOI operations.
 */

#ifndef LETTUCE_ARCH_GIC_H
#define LETTUCE_ARCH_GIC_H

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define GICD_BASE            UINT64_C(0x08000000)
#define GICC_BASE            UINT64_C(0x08010000)

#define GIC_INTID_VTIMER     27u /* Virtual Timer PPI 11 */
#define GIC_INTID_PTIMER     30u /* Physical Timer PPI 14 */
#define GIC_SPURIOUS_INTID   1023u

void lettuce_gic_init(void);
void lettuce_gic_enable_interrupt(uint32_t intid);
void lettuce_gic_disable_interrupt(uint32_t intid);
void lettuce_gic_set_priority(uint32_t intid, uint8_t priority);
uint32_t lettuce_gic_acknowledge(void);
void lettuce_gic_end_of_interrupt(uint32_t intid);

#endif /* LETTUCE_ARCH_GIC_H */
