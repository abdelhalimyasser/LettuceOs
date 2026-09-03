/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/scheduler/eevdf.h
 *
 * Purpose:
 *   Declares EEVDF policy state and hooks for the scheduler policy interface.
 *
 * Design:
 *   The API exposes scheduling choice and accounting only, never architectural
 *   context-switch or address-space controls.
 */

#ifndef LETTUCE_SCHED_EEVDF_H
#define LETTUCE_SCHED_EEVDF_H

#pragma once

#include "policy.h"

extern const LettuceSchedPolicyOps g_lettuce_sched_eevdf_ops;

/* Diagnostic & introspection helpers for EEVDF validation */
uint64_t lettuce_eevdf_get_vsys(void);
bool lettuce_eevdf_is_eligible(const LettuceTask *task);
uint64_t lettuce_eevdf_calc_deadline(const LettuceTask *task);
int64_t lettuce_eevdf_get_lag(const LettuceTask *task);

#endif /* LETTUCE_SCHED_EEVDF_H */
