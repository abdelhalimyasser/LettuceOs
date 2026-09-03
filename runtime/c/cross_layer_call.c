/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: runtime/c/cross_layer_call.c
 *
 * Purpose:
 *   Provides the C runtime wrapper for Cross-Layer mediated calls.
 *
 * Design:
 *   The wrapper submits request data; EL1 derives caller identity and owns
 *   capability validation and protection-domain transition decisions.
 */

#include "../../include/lettuce/message.h"
#include "../../kernel/include/kernel.h"

LettuceStatus lettuce_cross_layer_call(const LettuceCallMessage *message)
{
	return lettuce_cross_layer_gate(message);
}
