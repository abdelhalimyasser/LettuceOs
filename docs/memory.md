# Memory, Domains, and Shared Buffers

On ARM64, [`mmu.c`](../kernel/arch/arm64/mmu.c) constructs separate domain
translation roots. A domain identifies an address space; a layer classifies a
service; an ASID tags TLB entries for a root. None of these authorizes a call:
the capability engine does that.

User descriptors are non-global. A steady-state domain switch writes the
target root and ASID to `TTBR0_EL1` between `dsb ish` and `isb`; targeted and
global invalidation helpers are used when mappings change. The ARM64 test
harness exercises private-domain access, foreign-page faults, kernel-memory
denial, and ASID isolation under QEMU TCG.

## Fixed allocator

[`memory/allocator/fixed.c`](../memory/allocator/fixed.c) manages a static
64-page pool with 4 KiB pages. Handles encode a slot and generation. Release
requires the allocation head, matching generation, and owning domain; reused
slots advance generation so stale handles fail.

## Shared buffers

[`memory/shared/buffer.c`](../memory/shared/buffer.c) contains 16 statically
allocated buffers, each with up to 4 KiB payload. Creation requires the
authoritative current service to match the declared owner. Read/write access
requires the matching configured capability and a capability-table check.
Payloads are zeroed on reuse and revoke; revoke advances the generation.

In the host prototype, an already-returned raw pointer remains an ordinary host
pointer until real mapping boundaries are active. API revocation rejects later
lookups, but it is not retroactive hardware unmapping in host mode.

## DynamicArray

[`shared/dynamic_array/`](../shared/dynamic_array/) exists and implements a
heap-backed circular array. It uses `malloc` and `free`, returns failure rather
than terminating on allocation failure, and checks growth arithmetic. It is a
utility for non-hot-path use and must not be used in IRQ, scheduler,
context-switch, or capability paths.
