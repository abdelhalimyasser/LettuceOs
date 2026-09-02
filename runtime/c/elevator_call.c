/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../../include/lettuce/message.h"
#include "../../kernel/include/kernel.h"

LettuceStatus lettuce_elevator_call(const LettuceCallMessage *message)
{
    return lettuce_elevator_gate(message);
}