/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/arch/arm64/irq.h
 *
 * Purpose:
 *   Declares the ARM64 IRQ handler interface used by vector-entry assembly.
 *
 * Design:
 *   The handler coordinates interrupt sources with kernel mechanisms; policy
 *   selection remains in scheduler code.
 */

#ifndef LETTUCE_ARCH_IRQ_H
#define LETTUCE_ARCH_IRQ_H

#pragma once

#include <stdint.h>
#include <stdbool.h>

void lettuce_arch_irq_enable(void);
void lettuce_arch_irq_disable(void);
uint64_t lettuce_arch_irq_save(void);
void lettuce_arch_irq_restore(uint64_t flags);

void lettuce_preempt_disable(void);
void lettuce_preempt_enable(void);
bool lettuce_preempt_is_enabled(void);

#endif /* LETTUCE_ARCH_IRQ_H */
