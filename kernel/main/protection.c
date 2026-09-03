/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/main/protection.c
 *
 * Purpose:
 *   Maintains portable logical protection-domain state and delegates concrete
 *   domain entry to the architecture layer.
 *
 * Key invariants:
 *   - Domain identifiers are validated before transition requests.
 *   - This layer does not substitute logical state for hardware isolation.
 */

#include <stdint.h>

#include "../include/protection.h"

#if defined(__aarch64__)
#include "../include/arch.h"

LettuceDomainId lettuce_protection_enter(LettuceDomainId target_domain)
{
    return lettuce_mmu_enter(target_domain);
}

void lettuce_protection_leave(LettuceDomainId previous_domain)
{
    lettuce_mmu_leave(previous_domain);
}

LettuceDomainId lettuce_protection_current_domain(void)
{
    return lettuce_mmu_current_domain();
}
#else
static LettuceDomainId g_current_domain = LETTUCE_DOMAIN_ID_INVALID;

LettuceDomainId lettuce_protection_enter(LettuceDomainId target_domain)
{
    const LettuceDomainId previous_domain = g_current_domain;
    if (target_domain != LETTUCE_DOMAIN_ID_INVALID)
    {
        g_current_domain = target_domain;
    }

    __asm__ __volatile__("" ::: "memory");
    return previous_domain;
}

void lettuce_protection_leave(LettuceDomainId previous_domain)
{
    g_current_domain = previous_domain;
    __asm__ __volatile__("" ::: "memory");
}

LettuceDomainId lettuce_protection_current_domain(void)
{
    return g_current_domain;
}
#endif
