/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/arch/arm64/pac.S
 *
 * Purpose:
 *   Implements ARM64 pointer-authentication helpers for protected continuation
 *   and control-flow state.
 *
 * Execution context:
 *   EL1 architecture support code.
 *
 * Key invariants:
 *   PAC hardens continuation integrity; it is not capability authorization.
 */

.arch armv8.3-a+pauth
.text
.align 2

.global lettuce_pac_init
.type lettuce_pac_init, @function
lettuce_pac_init:
    /* Load a 128-bit key into APIAKey */
    ldr x0, =0x0123456789abcdef
    ldr x1, =0xfedcba9876543210
    msr s3_0_c2_c1_0, x0   /* APIAKeyLo_EL1 */
    msr s3_0_c2_c1_1, x1   /* APIAKeyHi_EL1 */

    /* Enable Instruction Address Authentication using Key A in SCTLR_EL1 (bit 31, EnIA) */
    mrs x2, sctlr_el1
    orr x2, x2, #(1 << 31)
    msr sctlr_el1, x2
    isb
    ret

.global lettuce_pac_sign
.type lettuce_pac_sign, @function
lettuce_pac_sign:
    /* x0 = pointer, x1 = modifier */
    pacia x0, x1
    ret

.global lettuce_pac_auth
.type lettuce_pac_auth, @function
lettuce_pac_auth:
    /* x0 = signed pointer, x1 = modifier */
    autia x0, x1
    ret
