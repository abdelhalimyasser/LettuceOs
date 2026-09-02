# Communication Model

SPDX-License-Identifier: Apache-2.0

This document captures the communication patterns between isolated domains and layers.

## Same-layer protected call

The same-layer protected call is the first concrete fast path in the prototype. It is intended for calls where the caller and target are both in the same logical layer, such as L3 camera -> L3 display, and the service uses a kernel-owned capability to authorize the operation.

The flow is intentionally small:

1. resolve the trusted current service from kernel execution context
2. look up the target service in the registry
3. verify the target is active and in the same layer as the caller
4. validate the capability for the exact target, operation, and resource
5. resolve the registered operation entry for the target service
6. enter the target domain through the logical protection hook
7. invoke the registered function
8. restore the previous domain state

This differs from a direct function call because it does not accept an arbitrary caller-supplied pointer or target address. The kernel owns the registry and dispatch table, and the runtime only supplies the service ID, operation ID, resource ID, and capability handle.

The prototype currently models the protection transition as a logical/emulated domain transition only. No real POE, MMU, PAC, or MTE enforcement is attempted in this milestone.

## Current implementation status

- same-layer validation is performed in the validation layer
- capability checks still rely on the kernel-trusted current service identity
- the protection-domain transition is an emulated software hook and compiler barrier
- no real ARM64 isolation is claimed
- cross-layer IPC and Elevator are intentionally out of scope for this milestone

Cross-layer Stairs now use the same direct target lookup and operation resolution, but require the target layer to differ. The hierarchy is classification only: an authorized L3 to L1 call goes directly to L1 and does not route through L2.

Elevator is a separate policy path requiring both `CALL` and `CRITICAL` for the exact operation. The current host implementation is a portable C fallback; no architecture-specific transition is claimed.

## Concurrency assumption

The prototype remains serialized and single-threaded. There are no locks or per-core state changes in this milestone.
