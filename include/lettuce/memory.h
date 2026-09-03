/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: include/lettuce/memory.h
 *
 * Purpose:
 *   Declares fixed-pool memory and shared-buffer handles used by Lettuce
 *   services.
 *
 * Design:
 *   Access APIs bind allocation and shared-buffer operations to domains and
 *   capabilities; implementation storage is owned elsewhere.
 */

#ifndef LETTUCE_MEMORY_H
#define LETTUCE_MEMORY_H

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <lettuce/capability.h>
#include <lettuce/errors.h>
#include <lettuce/types.h>

typedef uint32_t LettuceMemoryHandle;
typedef uint32_t LettuceSharedBufferHandle;

#define LETTUCE_MEMORY_HANDLE_INVALID ((LettuceMemoryHandle)0u)
#define LETTUCE_SHARED_BUFFER_INVALID ((LettuceSharedBufferHandle)0u)

#define LETTUCE_MEMORY_PAGE_SIZE 4096u
#define LETTUCE_MEMORY_POOL_SIZE (64u * LETTUCE_MEMORY_PAGE_SIZE)
#define LETTUCE_SHARED_BUFFER_SIZE 4096u
#define LETTUCE_SHARED_BUFFER_COUNT 16u
#define LETTUCE_SHARED_READ_OPERATION UINT32_C(0x100)
#define LETTUCE_SHARED_WRITE_OPERATION UINT32_C(0x101)

LettuceStatus lettuce_memory_init(void);
LettuceStatus lettuce_memory_allocate(LettuceDomainId domain, size_t size, LettuceMemoryHandle *handle);
LettuceStatus lettuce_memory_release(LettuceMemoryHandle handle, LettuceDomainId domain);
void *lettuce_memory_data(LettuceMemoryHandle handle, LettuceDomainId domain);

LettuceStatus lettuce_shared_buffer_init(void);
LettuceStatus lettuce_shared_buffer_create(
    LettuceServiceId owner,
    LettuceResourceId resource,
    size_t size,
    LettuceCapabilityHandle read_capability,
    LettuceCapabilityHandle write_capability,
    LettuceSharedBufferHandle *handle);
LettuceStatus lettuce_shared_buffer_access(
    LettuceSharedBufferHandle handle,
    LettuceCapabilityHandle capability,
    bool write,
    void **data,
    size_t *size);
LettuceStatus lettuce_shared_buffer_revoke(LettuceSharedBufferHandle handle);

#endif
