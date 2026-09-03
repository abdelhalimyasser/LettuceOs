/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: ipc/elevator/elevator.c
 *
 * Purpose:
 *   Implements the policy-mediated Elevator request entry point.
 *
 * Flow:
 *   Request message -> Elevator policy authorization -> target resolution ->
 *   protected context transition.
 *
 * This file does not:
 *   Permit a user-controlled privileged jump or replace C authorization.
 */

#include "../../kernel/include/kernel.h"
#include "../../kernel/include/context.h"
#include "../../kernel/include/protection.h"

LettuceStatus lettuce_elevator_gate(const LettuceCallMessage *message)
{
    LettuceCallResolution resolution;
    const LettuceStatus status = lettuce_elevator_policy(message, &resolution);
    if (status != LETTUCE_STATUS_OK)
        return status;

    const LettuceExecutionContext previous_context =
        lettuce_context_enter(resolution.target->id, resolution.target->domain);
    const LettuceStatus result = resolution.entry->entry();
    lettuce_context_leave(previous_context);

    return result;
}
