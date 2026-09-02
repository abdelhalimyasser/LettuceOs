/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _MESSAGE_H
#define _MESSAGE_H

#pragma once

#include <lettuce/capability.h>
#include <lettuce/errors.h>
#include <lettuce/types.h>

typedef struct LettuceCallMessage
{
	LettuceServiceId target_service_id;
	LettuceOperationId operation_id;
	LettuceResourceId resource_id;
	LettuceCapabilityHandle capability_handle;
} LettuceCallMessage;

LettuceStatus lettuce_cross_layer_call(const LettuceCallMessage *message);
LettuceStatus lettuce_elevator_call(const LettuceCallMessage *message);

#endif
