# AGENTS.md — Canonical Engineering & Agent Instructions

This document is the authoritative instruction file for all AI coding agents, autonomous contributors, and human developers contributing to the **Lettuce ARM64 Operating System**.

---

## 1. Project Mission & Architectural Tenets

Lettuce is a capability-based research microkernel designed for ARM64. Its primary research objective is investigating how modern processor security primitives (ASIDs, MMU, PAC, MTE, POE) can be combined with a layered capability architecture to provide low-latency mediated inter-process communication without relinquishing hardware memory isolation.

### Core Non-Negotiable Tenets
1. **Never Fake Hardware Features:** If an architectural feature (e.g., POE or physical MTE tag RAM) is unsupported or non-functional in the running CPU model, accurately report it as unsupported. Never write software emulators and label them with hardware architectural names.
2. **Zero Dynamic Allocation in Hot Paths:** Never allocate dynamic memory in interrupt entry/exit, context switching, the scheduler loop, capability checks, or Elevator transitions.
3. **Strict Privilege Isolation:** Microkernel code and page table roots execute strictly at **EL1**. Synthetic services and user tasks execute strictly at **EL0**.
4. **Authoritative Caller Identity:** Never trust user-supplied registers (e.g., `x0`, `x1`) for caller `ServiceId` or `DomainId`. The kernel must resolve caller identity authoritatively from internal execution state (`current_service_id`).
5. **No Monolithic Bloat:** Do not implement monolithic OS abstractions (e.g., `fork()`, `exec()`, `signals`, `mmap()`, full POSIX pthreads, virtual file systems, network stacks) unless an explicit architectural mandate is given.

---

## 2. Language Responsibilities & Placement Matrix

Code must be placed in the proper language tier based on architectural necessity:

```text
 ┌──────────────────────────────────────────────────────────────┐
 │ ARM64 Assembly (.S)                                          │
 │ - Vector table entries and exception returns (ERET)          │
 │ - Callee-saved context switch primitives                     │
 │ - PAC signing and verification gates (pacia/autia)           │
 │ - Fast Elevator register/MMU transition gate (elevator.S)    │
 │ - DAIF interrupt masking/unmasking (msr daifclr/daifset)     │
 ├──────────────────────────────────────────────────────────────┤
 │ C99 / C11 (.c / .h)                                          │
 │ - MMU page table creation and translation logic              │
 │ - GICv2 interrupt controller driver and timer logic          │
 │ - Task state management and Round-Robin scheduler            │
 │ - Capability creation, verification, and revocation          │
 │ - Service dispatcher and invocation policy                   │
 │ - POSIX-lite syscall entry and file descriptor tables        │
 ├──────────────────────────────────────────────────────────────┤
 │ Safe Rust (.rs)                                              │
 │ - User-space service SDK and client-side runtimes            │
 │ - Ergonomic, memory-safe POSIX-lite wrappers                 │
 │ - Strongly typed TaskId and CapabilityHandle abstractions    │
 ├──────────────────────────────────────────────────────────────┤
 │ C++ (Excluded from Core)                                     │
 │ - Intentionally excluded from the privileged core to keep    │
 │   runtime, allocation, exception, and ABI policy explicit.   │
 └──────────────────────────────────────────────────────────────┘
```

---

## 3. Security & Invariant Checklist

Before committing any modifications, agents must verify that all invariants remain intact:

- [ ] **Capability Invariant:** Is `lettuce_capability_check()` invoked before any protection domain transition?
- [ ] **Elevator Invariant:** Does the Elevator path require both `LETTUCE_CAP_CALL` and `LETTUCE_CAP_CRITICAL`? Does all authorization remain in C?
- [ ] **MMU Invariant:** Are all user pages marked Non-Global (`nG = 1`)? Are supervisor RAM pages marked Global (`nG = 0`), Unprivileged Execute Never (`UXN`), and EL1 Read/Write?
- [ ] **Barrier Invariant:** Is every write to `TTBR0_EL1` preceded by `dsb ish` and followed by `isb`? Never remove architectural barriers without formal proof.
- [ ] **ASID Invariant:** Does `lettuce_mmu_enter()` preserve the 16-bit ASID in `TTBR0_EL1[63:48]`?
- [ ] **PAC Invariant:** Are return continuations on the context stack signed with `pacia` and authenticated with `autia`? Does corrupted authentication synchronously fault?
- [ ] **Preemption Invariant:** Are kernel critical sections guarded by `lettuce_preempt_disable()` and `lettuce_preempt_enable()`?
- [ ] **POSIX Invariant:** Does the system call dispatcher validate user pointers and reject supervisor memory (`0x40000000 - 0x401fffff`) with `-EFAULT`?

---

## 4. Verification & Testing Requirements

Every change must be validated across three verification tiers:

### 1. Host Regression Suite
```bash
make test
make check
```
- All 11 host unit test suites must pass (`capability_unit`, `capability_security`, `same_layer_unit`, `cross_layer_unit`, `elevator_unit`, `memory_unit`, `context_nested_unit`, `dynamic_array_unit`, `task_scheduler_unit`, `sched_eevdf_unit`, `posix_unit`).
- `cargo check` and `git diff --check` must produce zero warnings.

### 2. Bare-Metal ARM64 Cross-Build & QEMU Boot
```bash
bash scripts/build-arm64.sh
bash scripts/run-qemu.sh
```
- The cross-build must compile cleanly under Clang/LLD without warnings.
- The QEMU output must successfully execute all 25 runtime tests and print:
  ```text
  All 25 ARM64 Execution/Runtime Foundation Tests Passed!
  EXECUTION RUNTIME FOUNDATION PASS
  ```

### 3. Empirical Benchmark Integrity
- Never alter benchmark code to artificially deflate latency.
- Never compare QEMU TCG software emulation timings directly against physical silicon cycle counts.
- Treat Generic Counter ticks under QEMU TCG as emulator-relative; GitHub Actions timing output is regression evidence, not publication-quality performance data.
- Clearly annotate all performance data with sample sizes, percentiles (p50, p95, p99), and standard deviations.

---

## 5. Prohibited Agent Behaviors

1. **DO NOT** add dynamic heap allocations (`malloc`, `free`, `realloc`, `Vec`, `Box`) to interrupt handlers, context switches, scheduling loops, or capability paths.
2. **DO NOT** weaken capability bitmask checks or skip permission validation for "convenience".
3. **DO NOT** remove `isb` or `dsb` barriers from MMU or system-register operations.
4. **DO NOT** silently rewrite or overwrite historical benchmark numbers in `results/`.
5. **DO NOT** add monolithic operating system subsystems (e.g., networking, file systems, `fork()`).
6. **DO NOT** invent non-existent hardware features or simulate them in software under hardware names.
