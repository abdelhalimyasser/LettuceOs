/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: ipc/cross_layer/message.c
 *
 * Purpose:
 *   Validates the message and service state used by Cross-Layer mediation.
 *
 * Responsibilities:
 *   - Resolve the authoritative caller and requested target.
 *   - Require an active, differently classified target and CALL capability.
 *   - Return the dispatch resolution without executing it.
 */

#include "../../include/lettuce/message.h"

_Static_assert(sizeof(LettuceCallMessage) == 16, "LettuceCallMessage must remain four ABI words.");
