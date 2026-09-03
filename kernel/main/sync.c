/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/main/sync.c
 *
 * Purpose:
 *   Implements bounded preemption-disable nesting for kernel critical
 *   sections.
 *
 * Key invariants:
 *   - Nesting never underflows.
 *   - Critical call paths use this state to prevent scheduler preemption.
 */
