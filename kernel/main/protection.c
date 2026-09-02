/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include "../include/protection.h"

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
