/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/arch/arm64/gic.c
 *
 * Purpose:
 *   Implements GICv2 initialization, interrupt acknowledge, and end-of-
 *   interrupt operations for the ARM64 platform.
 *
 * Flow:
 *   Interrupt source -> GIC acknowledge -> EL1 handler -> EOI.
 */

#include "gic.h"

#define GICD_REG(off) (*(volatile uint32_t *)(uintptr_t)(GICD_BASE + (off)))
#define GICC_REG(off) (*(volatile uint32_t *)(uintptr_t)(GICC_BASE + (off)))

#define GICD_CTLR       0x000u
#define GICD_TYPER      0x004u
#define GICD_ISENABLER  0x100u
#define GICD_ICENABLER  0x180u
#define GICD_IPRIORITYR 0x400u
#define GICD_ITARGETSR  0x800u

#define GICC_CTLR       0x000u
#define GICC_PMR        0x004u
#define GICC_IAR        0x00cu
#define GICC_EOIR       0x010u

void lettuce_gic_init(void)
{
    /* 1. Disable Distributor while configuring */
    GICD_REG(GICD_CTLR) = 0u;

    /* 2. Disable all SPIs and PPIs initially (first 64 interrupts) */
    GICD_REG(GICD_ICENABLER + 0x00u) = 0xffffffffu;
    GICD_REG(GICD_ICENABLER + 0x04u) = 0xffffffffu;

    /* 3. Set priority mask on CPU interface to allow all priorities (0xff) */
    GICC_REG(GICC_PMR) = 0xffu;

    /* 4. Enable CPU interface (Group 0 signaling) */
    GICC_REG(GICC_CTLR) = 1u;

    /* 5. Enable Distributor (Group 0 distribution) */
    GICD_REG(GICD_CTLR) = 1u;
}

void lettuce_gic_enable_interrupt(uint32_t intid)
{
    const uint32_t reg_idx = intid / 32u;
    const uint32_t bit_idx = intid % 32u;
    GICD_REG(GICD_ISENABLER + (reg_idx * 4u)) = (1u << bit_idx);
}

void lettuce_gic_disable_interrupt(uint32_t intid)
{
    const uint32_t reg_idx = intid / 32u;
    const uint32_t bit_idx = intid % 32u;
    GICD_REG(GICD_ICENABLER + (reg_idx * 4u)) = (1u << bit_idx);
}

void lettuce_gic_set_priority(uint32_t intid, uint8_t priority)
{
    volatile uint8_t *prio_regs = (volatile uint8_t *)(uintptr_t)(GICD_BASE + GICD_IPRIORITYR);
    prio_regs[intid] = priority;
}

uint32_t lettuce_gic_acknowledge(void)
{
    return GICC_REG(GICC_IAR) & 0x3ffu;
}

void lettuce_gic_end_of_interrupt(uint32_t intid)
{
    GICC_REG(GICC_EOIR) = intid & 0x3ffu;
}
