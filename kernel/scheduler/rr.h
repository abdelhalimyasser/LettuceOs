/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/scheduler/rr.h
 *
 * Purpose:
 *   Declares the Round-Robin scheduler policy hook.
 *
 * Provides:
 *   A policy descriptor consumed by the common scheduler mechanism.
 */

#ifndef LETTUCE_SCHED_RR_H
#define LETTUCE_SCHED_RR_H

#pragma once

#include "policy.h"

extern const LettuceSchedPolicyOps g_lettuce_sched_rr_ops;

#endif /* LETTUCE_SCHED_RR_H */
