/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KERNEL_H
#define KERNEL_H

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <lettuce/service.h>

#define LETTUCE_SERVICE_TABLE_SIZE 256u

typedef struct LettuceServiceRegistryEntry
{
    LettuceServiceId id;
    LettuceServiceDescriptor descriptor;
    bool active;
} LettuceServiceRegistryEntry;

bool lettuce_service_registry_init(void);
bool lettuce_service_registry_register(LettuceServiceDescriptor descriptor);
bool lettuce_service_registry_unregister(LettuceServiceId service_id);
const LettuceServiceDescriptor *lettuce_service_registry_lookup(LettuceServiceId service_id);
bool lettuce_service_registry_is_active(LettuceServiceId service_id);
bool lettuce_service_registry_validate(LettuceServiceId service_id);

LettuceServiceId current_service_id(void);
void set_current_service_id(LettuceServiceId service_id);
void kernel_set_current_service_id(LettuceServiceId service_id);

#endif
