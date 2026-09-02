/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KERNEL_H
#define KERNEL_H

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <lettuce/capability.h>
#include <lettuce/errors.h>
#include <lettuce/message.h>
#include <lettuce/service.h>

#define LETTUCE_SERVICE_TABLE_SIZE 256u
#define LETTUCE_DISPATCH_OPERATION_LIMIT 64u

typedef struct LettuceDispatchEntry
{
    LettuceStatus (*entry)(void);
    bool active;
} LettuceDispatchEntry;

typedef struct LettuceServiceRegistryEntry
{
    LettuceServiceDescriptor descriptor;
    LettuceDispatchEntry operations[LETTUCE_DISPATCH_OPERATION_LIMIT];
} LettuceServiceRegistryEntry;

typedef struct LettuceSameLayerResolution
{
    const LettuceServiceDescriptor *caller;
    const LettuceServiceDescriptor *target;
    const LettuceDispatchEntry *entry;
} LettuceSameLayerResolution;

bool lettuce_service_registry_init(void);
bool lettuce_service_registry_register(LettuceServiceDescriptor descriptor);
bool lettuce_service_registry_unregister(LettuceServiceId service_id);
const LettuceServiceDescriptor *lettuce_service_registry_lookup(LettuceServiceId service_id);
LettuceServiceRegistryEntry *lettuce_service_registry_entry_mutable(LettuceServiceId service_id);
bool lettuce_service_registry_is_active(LettuceServiceId service_id);
bool lettuce_service_registry_validate(LettuceServiceId service_id);

bool lettuce_dispatch_register(LettuceServiceId service_id, LettuceOperationId operation_id, LettuceStatus (*entry)(void));
bool lettuce_dispatch_unregister(LettuceServiceId service_id, LettuceOperationId operation_id);
const LettuceDispatchEntry *lettuce_dispatch_lookup(LettuceServiceId service_id, LettuceOperationId operation_id);

LettuceStatus lettuce_same_layer_validate(
    LettuceServiceId target_service_id,
    LettuceOperationId operation_id,
    LettuceResourceId resource_id,
    LettuceCapabilityHandle capability_handle,
    LettuceSameLayerResolution *resolution);

LettuceStatus lettuce_same_layer_gate(
    LettuceServiceId target_service_id,
    LettuceOperationId operation_id,
    LettuceResourceId resource_id,
    LettuceCapabilityHandle capability_handle);

LettuceStatus lettuce_cross_layer_gate(const LettuceCallMessage *message);
LettuceStatus lettuce_elevator_gate(const LettuceCallMessage *message);
LettuceStatus lettuce_elevator_policy(const LettuceCallMessage *message, LettuceSameLayerResolution *resolution);

LettuceServiceId current_service_id(void);
void set_current_service_id(LettuceServiceId service_id);
void kernel_set_current_service_id(LettuceServiceId service_id);

#endif
