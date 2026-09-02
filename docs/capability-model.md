# Capability Model

SPDX-License-Identifier: Apache-2.0

This document describes the role of explicit capability checks and permission boundaries.

## Same-layer authorization model

Capabilities remain kernel-owned objects. Services do not generate or mutate capability metadata directly. A same-layer call is only allowed if the capability matches the current trusted caller, the exact target service, the exact operation, and the exact resource.

The same-layer validator requires the following:

- caller exists in the service registry
- target exists in the service registry
- caller is active
- target is active
- caller and target are in the same layer
- capability is valid and unrevoked
- capability owner matches the current runtime execution context
- capability target matches the target service
- capability operation contains the requested operation
- capability resource matches the requested resource

The prototype does not accept caller identity from user space. The authoritative caller identity comes from the kernel's current-service execution context, which is the trusted boundary for the service runtime.

The prototype also keeps stale and revoked capabilities invalid by generation checks in the kernel capability table. This preserves the kernel-managed authority model without duplicating validation logic in the runtime layer.

## Emulation note

This milestone does not provide real memory or permission isolation. The protection transition is logically emulated and is not a hardware-backed isolation boundary.

Capabilities now carry one exact logical operation ID in addition to the generic permission bits. A `CALL` permission for `SubmitFrame` therefore cannot authorize `ResetDisplay`. Capability ownership and generation checks remain kernel-managed; the caller argument is not part of the check API because identity comes from the trusted current-service context.

Memory and shared-buffer prototypes use fixed tables and generation-bearing handles. Shared access requires a matching read or write capability and revoked handles remain unusable.
