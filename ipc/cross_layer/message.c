/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../../include/lettuce/message.h"

_Static_assert(sizeof(LettuceCallMessage) == 16, "LettuceCallMessage must remain four ABI words.");
