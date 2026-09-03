/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/arch/arm64/timer.h
 *
 * Purpose:
 *   Declares ARM Generic Timer operations used by EL1 scheduling support.
 *
 * Design:
 *   Timer configuration drives interrupt delivery; it does not implement
 *   scheduler policy.
 */

#ifndef LETTUCE_ARCH_TIMER_H
#define LETTUCE_ARCH_TIMER_H

#pragma once

#include <stdint.h>
#include <stdbool.h>

uint64_t lettuce_arch_counter_read(void);
uint64_t lettuce_arch_counter_frequency(void);

void lettuce_timer_init(uint32_t hz);
void lettuce_timer_rearm(void);
void lettuce_timer_disable(void);
uint64_t lettuce_timer_get_ticks(void);
void lettuce_timer_irq_handler(void);

#endif /* LETTUCE_ARCH_TIMER_H */
