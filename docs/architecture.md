# Architecture

SPDX-License-Identifier: Apache-2.0

This document captures the intended project boundaries for the Lettuce research OS scaffold. It is intentionally a design note for the current tree, not an implementation plan.

## Architectural responsibility by tree

- include/lettuce/
  - Public-facing interfaces and ABI-level contracts consumed by service code, runtime wrappers, and external users of the system.
  - This tree defines the stable boundary between the system and its callers; it should not contain kernel-private enforcement details or privileged internals.
  - The public capability interface is a contract, not a kernel implementation detail.

- kernel/main/
  - The minimal privileged execution core for the system.
  - This code owns the enforcement path for traps, dispatch, scheduling hooks, capability validation, context switching, and the protected entry points that are trusted by the rest of the system.

- kernel/include/
  - Kernel-private headers and internal privileged interfaces that are not part of the public service ABI.
  - This is the correct location for kernel-owned internal definitions such as capability_internal.h; these definitions must not be exposed via the public include/lettuce tree.

- kernel/arch/arm64/
  - ARM64-specific privileged implementation: entry/exit stubs, context switching, MMU setup, PAC hardening hooks, MTE and memory-safety support, POE/POE2 overlays, low-level CPU/timer handling, and architecture-specific protected fast paths.
  - This layer is the hardware boundary for the privileged kernel, not the general service model.

- runtime/c/
  - Service-facing C wrappers and glue code that present a controlled interface to the kernel and communication subsystems.
  - These are not privileged enforcement code; they are the runtime-facing wrapper layer used by ordinary services.

- runtime/rust/
  - Higher-level runtime and native service components that prefer Rust for isolation, safer data handling, and modern service construction.
  - This tree is intended for newer service implementations and runtime support code, especially for L3-native components.

- ipc/
  - Communication implementation itself: protected-call mechanisms, message transport, same-layer interaction, cross-layer call handling, and the lower-level plumbing used to route requests.
  - This directory is about the implementation of communication paths, not the public interface definitions.

- memory/
  - Generic memory mechanisms and libraries used across the system: allocator logic, memory domains, tagging helpers, and generic runtime memory policy primitives.
  - The actual memory policy service belongs in the L1 layer, not here.

- security/
  - Generic security mechanisms and reusable policy logic that are not inherently the privileged core of the kernel.
  - This tree contains general-purpose security helpers and system security abstractions.

- layers/
  - Layered service organization for the operating system.
  - L1: critical control and resource services; L2: performance, scheduling, virtualization, and abstraction; L3: native OS-facing services; L4: compatibility, legacy, and external services.
  - The important boundary rule is that generic support code and security infrastructure stay in the shared trees, while layer-specific policy remains in the corresponding L1/L2/L3/L4 directories.

- layers/l1/memory/*
  - Memory policy and memory-management service logic for the critical L1 layer.
  - This is not the same as the generic memory library code under memory/.

- layers/l1/security/*
  - The critical security service in L1, responsible for the most important security policy enforcement and privileged trust decisions.
  - This is distinct from the generic security utilities in security/.

- tests/
  - Validation code, integration checks, fault tests, and unit-level verification for the research architecture.
  - Tests validate platform behavior and structural guarantees, but they are not part of the runtime or kernel implementation itself.

- benchmarks/
  - Evaluation and performance harnesses for measuring communication overhead, protected-call cost, memory behavior, system service latency, and other architectural trade-offs.
  - These are research instrumentation and performance tools rather than product code.

## System boundary rules

The project is intentionally organized around clear separation of responsibilities:

- runtime/c/* = service-facing wrappers
- ipc/* = communication implementation
- kernel/* = privileged enforcement and protected execution paths
- memory/* = generic memory mechanisms and libraries
- layers/l1/memory/* = memory policy/service
- security/* = generic security mechanisms
- layers/l1/security/* = critical security service

## API boundary rule for capability internals

The file include/lettuce/capability_internal.h must not be treated as a public or semi-public internal-kernel API. It is part of the public include tree and should remain free of kernel-private implementation details.

Kernel-private capability definitions belong in kernel/include/capability_internal.h, where they remain visible only to the privileged kernel and its trusted internal components.

This separation preserves the difference between:

1. the public ABI contract used by service code and runtime wrappers, and
2. the private implementation substrate used by the privileged kernel to enforce authorization.

The design should keep that distinction explicit and stable throughout the prototype.
