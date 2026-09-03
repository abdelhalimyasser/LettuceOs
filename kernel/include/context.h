/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/include/context.h
 *
 * Purpose:
 *   Defines logical service execution contexts and context-entry interfaces.
 *
 * Flow:
 *   Current service/domain -> target context entry -> target work -> saved
 *   context restoration.
 */

#ifndef LETTUCE_CONTEXT_H
#define LETTUCE_CONTEXT_H

#pragma once

#include <lettuce/types.h>

#include "protection.h"

typedef struct LettuceExecutionContext
{
    LettuceServiceId service_id;
    LettuceDomainId domain_id;
} LettuceExecutionContext;

LettuceExecutionContext lettuce_context_enter(LettuceServiceId target_service_id, LettuceDomainId target_domain_id);
void lettuce_context_leave(LettuceExecutionContext previous_context);

#endif
