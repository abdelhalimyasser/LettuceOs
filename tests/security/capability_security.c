/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <stdint.h>

#include "../../include/lettuce/capability.h"
#include "../../kernel/include/capability_internal.h"
#include "../../kernel/include/kernel.h"

int main(void)
{
    lettuce_capability_init();
    set_current_service_id(100u);

    const LettuceCapabilityHandle valid = lettuce_capability_create(
        100u,
        200u,
        7u,
        LETTUCE_CAP_CALL | LETTUCE_CAP_SIGNAL | LETTUCE_CAP_CRITICAL,
        300u);

    assert(valid != LETTUCE_CAPABILITY_INVALID);
    assert(lettuce_capability_create(100u, 200u, 7u, (LettuceCapabilityOperation)(1u << 31), 300u) == LETTUCE_CAPABILITY_INVALID);
    assert(lettuce_capability_check(valid, 200u, 7u, LETTUCE_CAP_CALL, 300u));
    assert(lettuce_capability_check(valid, 200u, 7u, LETTUCE_CAP_SIGNAL, 300u));
    assert(lettuce_capability_check(valid, 200u, 7u, LETTUCE_CAP_CRITICAL, 300u));

    set_current_service_id(101u);
    assert(!lettuce_capability_check(valid, 200u, 7u, LETTUCE_CAP_CALL, 300u));

    set_current_service_id(100u);
    assert(!lettuce_capability_check(valid, 201u, 7u, LETTUCE_CAP_CALL, 300u));
    assert(!lettuce_capability_check(valid, 200u, 7u, LETTUCE_CAP_READ, 300u));
    assert(!lettuce_capability_check(valid, 200u, 7u, LETTUCE_CAP_CALL, 301u));

    assert(lettuce_capability_revoke(valid));
    assert(!lettuce_capability_check(valid, 200u, 7u, LETTUCE_CAP_CALL, 300u));

    return 0;
}
