# Evaluation Plan

SPDX-License-Identifier: Apache-2.0

This document defines the evaluation plan and benchmarking strategy for the architecture.

## Same-layer protected call benchmark

The host-side benchmark compares the cost of a direct C function call against the same-layer protected call path.

The benchmark measures:

- total elapsed time
- nanoseconds per call
- operations per second

The host/x86 results are intentionally not representative of final ARM64 behavior. The benchmark is a host-side measurement for the prototype's relative overhead and not a claim about the final protected call performance on the target architecture.

## Planned validation loop

The milestone validation steps are:

1. build the project
2. run unit tests for capability and same-layer behavior
3. run security tests for the capability model
4. execute the direct-call benchmark
5. execute the same-layer benchmark
6. record results and limitations

This prototype keeps the fast path serialized and single-threaded, and therefore does not represent a concurrent multi-core or multi-threaded production path.

Memory evaluation includes fixed-page domain ownership, invalid domain access, shared-buffer capability checks, revocation, and stale-handle behavior. The implementation is a research mechanism, not a complete virtual-memory manager.

ARM64 status is environment-dependent. `scripts/build-arm64.sh` uses an available AArch64 cross compiler, while `scripts/run-qemu.sh` runs an ELF only when both QEMU and an image are present. PAC, MTE, POE, and MMU experiments remain emulated or unimplemented until supported instructions and a bootable target are available.

The communication regression set measures direct C, capability checking, same-layer, cross-layer, and Elevator calls on the host. These measurements are latency/throughput indicators only and do not represent ARM64 hardware performance.

The separate custom allocator and garbage-collection project is intentionally not imported into the kernel. Communication paths retain fixed static tables and do not depend on malloc, dynamic arrays, or tracing GC.
