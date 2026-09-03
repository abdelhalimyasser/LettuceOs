/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: include/lettuce/message.h
 *
 * Purpose:
 *   Defines the compact mediated-call message shared by Cross-Layer and
 *   Elevator public entry points.
 *
 * Flow:
 *   Service request fields -> kernel-authoritative caller identity ->
 *   capability validation and route-specific dispatch.
 */

#ifndef LETTUCE_MESSAGE_H
#define LETTUCE_MESSAGE_H

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
