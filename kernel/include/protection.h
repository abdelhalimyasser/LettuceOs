/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/include/protection.h
 *
 * Purpose:
 *   Declares protection-domain transition hooks used by the portable kernel
 *   layer.
 *
 * Design:
 *   The interface describes policy-visible domain state; an architecture
 *   backend owns concrete MMU and address-space mechanics.
 */

#ifndef LETTUCE_PROTECTION_H
#define LETTUCE_PROTECTION_H

#pragma once

#include <lettuce/types.h>

/*
 * Logical/emulated domain transition hook.
 *
 * This prototype intentionally does not perform real ARM64 POE/MMU switching;
 * it only models the kernel-side policy transition expected by the same-layer
 * fast path and is safe to use as a software-only prototype barrier.
 */
LettuceDomainId lettuce_protection_enter(LettuceDomainId target_domain);
void lettuce_protection_leave(LettuceDomainId previous_domain);
LettuceDomainId lettuce_protection_current_domain(void);

#endif
