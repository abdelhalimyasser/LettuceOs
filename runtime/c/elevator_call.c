/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: runtime/c/elevator_call.c
 *
 * Purpose:
 *   Provides the C runtime wrapper for an Elevator mediated-call request.
 *
 * Design:
 *   This file does not authorize privileged transfer; the supervisor requires
 *   CALL and CRITICAL rights before entering the specialized path.
 */

#include "../../include/lettuce/message.h"
#include "../../kernel/include/kernel.h"

LettuceStatus lettuce_elevator_call(const LettuceCallMessage *message)
{
    return lettuce_elevator_gate(message);
}
