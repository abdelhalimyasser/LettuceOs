# Lettuce: A Layered ARM64 Research Operating System

[![Build & Test](https://github.com/abdelhalimyasser/LettuceOs/actions/workflows/ci.yml/badge.svg)](https://github.com/abdelhalimyasser/LettuceOs/actions/workflows/ci.yml)
[![ARM64 QEMU](https://github.com/abdelhalimyasser/LettuceOs/actions/workflows/arm64-qemu.yml/badge.svg)](https://github.com/abdelhalimyasser/LettuceOs/actions/workflows/arm64-qemu.yml)
[![Security Regression](https://github.com/abdelhalimyasser/LettuceOs/actions/workflows/security.yml/badge.svg)](https://github.com/abdelhalimyasser/LettuceOs/actions/workflows/security.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

**Lettuce** is a freestanding research microkernel prototype for 64-bit ARM (ARMv8-A / AArch64). Its core mission is investigating how modern processor security primitives—such as Address Space Identifiers (ASIDs), MMU hardware domains, and Pointer Authentication (PAC)—can be combined with a layered capability architecture to deliver fine-grained memory isolation, low-latency capability mediation, and deterministic preemptive scheduling without monolithic operating system bloat.

---

## 1. Project Overview

Modern operating systems face a persistent tension between isolation and communication performance:
- **Monolithic kernels** attain fast inter-module communication by running all services in a single privileged supervisor address space, but any memory corruption or driver vulnerability compromises total system integrity.
- **Traditional microkernels** isolate services into discrete address spaces, but cross-domain Inter-Process Communication (IPC) historically incurs steep latency penalties from hardware privilege crossings, page directory switches, and TLB flushes.

Lettuce explores an architectural middle ground on ARM64 by:
1. Executing unprivileged synthetic services strictly at **EL0** and the supervisor microkernel strictly at **EL1**.
2. Organizing system services into a **four-layer classification model** ($L_1$--$L_4$) with three explicit communication topologies (Same-Layer, Cross-Layer, Elevator).
3. Using dedicated **16-bit hardware ASIDs** and Non-Global (`nG=1`) page descriptors to eliminate global TLB flushes on steady-state protection domain crossings.
4. Protecting supervisor call-gate continuation state with cryptographic **Pointer Authentication (PAC)**.
5. Decoupling the **preemptive scheduling mechanism** (timer interrupts, GICv2, context swaps) from pluggable scheduling policies (**Round-Robin** and **EEVDF**).
6. Providing a minimal **POSIX-lite** system call interface and a memory-safe **Safe Rust runtime SDK** for user-space services.

---

## 2. Architecture Summary

### Privilege Model: EL0 Services & EL1 Supervisor

Lettuce enforces strict privilege separation:
- **EL1 (Supervisor Microkernel):** Holds the vector table (`VBAR_EL1`), page table roots, capability directories, task table, GICv2 interrupt handling, and scheduler state. Kernel physical RAM (`0x40000000 - 0x401fffff`) is marked Unprivileged Execute Never (`UXN`) and supervisor-only (`AP=0b01`).
- **EL0 (Synthetic Services & User Tasks):** Runs unprivileged code in thread mode (`EL0t`). Access to privileged system registers (e.g., `TTBR0_EL1`, `SCTLR_EL1`) is trapped synchronously by hardware.
- **Authoritative Caller Identity:** The kernel never trusts user-supplied registers (such as `x0` or `x1`) for caller identity. Identity is authoritatively resolved from supervisor execution context (`current_service_id`).

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                             EL0 USER SPACE                             │
 │                                                                        │
 │    ┌────────────────────┐   Same-Layer   ┌────────────────────┐        │
 │    │  Layer 3: Camera   │ <------------> │  Layer 3: Display  │        │
 │    │  (Domain A / 100)  │                │  (Domain B / 200)  │        │
 │    └─────────┬──────────┘                └────────────────────┘        │
 │              │                                                         │
 │              │ Cross-Layer                                             │
 │              ▼                                                         │
 │    ┌────────────────────┐                                              │
 │    │  Layer 2: Sensor   │ <..................................+         │
 │    │  (Domain C / 300)  │                                    :         │
 │    └────────────────────┘                                    : Elevator│
 │              │                                               : (Bypass)│
 │              + - - - - - - - - - - - - - - - - - - - - - - - +         │
 └───────────────────────────────────┬────────────────────────────────────┘
                                     │ SVC (#1, #2, #3, #5)
                                     ▼ ERET
 ┌────────────────────────────────────────────────────────────────────────┐
 │                        EL1 SUPERVISOR MICROKERNEL                      │
 │                                                                        │
 │    ┌────────────────────┐  ┌────────────────────┐  ┌──────────────┐    │
 │    │  Authoritative ID  │  │  Capability Table  │  │  Dispatcher  │    │
 │    │ (current_service)  │  │   (O(1) Flat)      │  │  (Mediator)  │    │
 │    └────────────────────┘  └────────────────────┘  └──────────────┘    │
 │                                                                        │
 │    ┌────────────────────┐  ┌────────────────────┐  ┌──────────────┐    │
 │    │     MMU & ASID     │  │    PAC Signing     │  │ GICv2/Timer  │    │
 │    │ (TTBR0 Translation)│  │ (pacia / autia)    │  │ (Scheduler)  │    │
 │    └────────────────────┘  └────────────────────┘  └──────────────┘    │
 └────────────────────────────────────────────────────────────────────────┘
```

### Layer Classification Model ($L_1$--$L_4$)

Services are categorized into four logical layers:
- **Layer 1 ($L_1$ — Core Resources):** Memory allocator, physical frame tracking, hardware watchdog, platform management.
- **Layer 2 ($L_2$ — Performance & Scheduling):** Scheduling policies, virtualization infrastructure, CPU management.
- **Layer 3 ($L_3$ — Native System Services):** Device drivers, audio, storage, camera, display pipelines.
- **Layer 4 ($L_4$ — External & Compatibility):** POSIX-lite shims, untrusted applications, third-party code.

> **Important Architectural Note:** Layers define *security classifications and authorization scopes*, not a mandatory sequential message pipeline. An $L_3$ service communicates directly with an $L_2$ service without traversing intermediate proxy layers.

### Three Mediated Communication Topologies

1. **Same-Layer Calls (Lateral):** Direct mediated invocation between services residing in the same layer ($L_i \leftrightarrow L_i$). Requires `LETTUCE_CAP_CALL` capability authorization and performs an ASID-aware domain switch.
2. **Cross-Layer Calls:** Invocation between distinct layers requiring caller and target to belong to distinct layers (`caller.layer != target.layer`), then performs capability-mediated target, operation, resource, and context validation, and preserves a full continuation frame on the kernel stack. Layers classify services; they do not impose directional routing policy, and an authorized target is directly invoked without intermediate layer hops.
3. **Elevator Calls (Capability-Gated Critical Path):** Capability-gated specialized transition path designed for urgent, latency-critical operations (e.g., camera capture driving real-time display composition). Requires both `LETTUCE_CAP_CALL` and `LETTUCE_CAP_CRITICAL` capability permissions. All capability authorization remains strictly in supervisor C logic; the ARM64 assembly gate (`elevator.S`) specializes the already-authorized register and MMU transition and does not authorize or bypass capability checks.

---

## 3. Implemented Features

- **Flat $O(1)$ Capability System:**
  - Token-based capability directory structured as flat arrays per protection domain.
  - Constant-time $O(1)$ capability lookups with permission bitmasks (`LETTUCE_CAP_CALL`, `LETTUCE_CAP_READ`, `LETTUCE_CAP_WRITE`, `LETTUCE_CAP_CRITICAL`, `LETTUCE_CAP_ELEVATOR`).
  - Generational handles preventing stale handle reuse.
- **Hardware MMU Isolation & 16-Bit ASIDs:**
  - 4 KiB translation granules with a 39-bit lower virtual address space (`T0SZ=25`, 512 GiB addressing).
  - Non-Global (`nG=1`) descriptor attributes mapped to distinct 16-bit ASIDs (`TTBR0_EL1[63:48]`).
  - Co-existing TLB entries across protection domains; domain switches execute with `dsb ish` and `isb` without broad `tlbi vmalle1` flushes.
  - Supervisor RAM (`0x40000000 - 0x401fffff`) protected with `UXN` and EL1 Read/Write attributes.
- **Pointer Authentication (PAC):**
  - Uses ARMv8.3-A PAC (`FEAT_PAC`) with key `APIAKey_EL1`.
  - Kernel call-gate return continuations on the context stack are signed with `pacia` using the stack pointer as contextual salt and verified with `autia`.
  - Continuation tampering causes authentication failure and synchronously triggers a Translation or PAC Fault (`EC=0x1c` or `0x25`).
- **GICv2 Interrupt Controller & ARM Generic Timer:**
  - Complete GICv2 distributor (`GICD`) and CPU interface (`GICC`) driver.
  - Routes ARM Generic Virtual Timer interrupts (PPI 27) to EL1 at a deterministic 100 Hz rate (10 ms quantum).
  - Explicit End-Of-Interrupt (`EOIR`) priority acknowledgment discipline before monitor return.
- **Decoupled Preemptive Scheduler:**
  - **Invariant Core:** Coordinates timer IRQs, register save/restore in bounded trap frames, sleeping task wakeups, and MMU/ASID domain handoffs.
  - **Static Task Table:** Statically dimensioned 16-task table (376 bytes per task, 6,016 bytes total footprint); zero dynamic memory allocation in hot paths.
  - **Reference Round-Robin (RR) Policy (`rr.c`):** Fast, deterministic $O(1)$ circular scan over ready tasks.
  - **EEVDF Policy (`eevdf.c`):** Implements Earliest Eligible Virtual Deadline First proportional fair-share scheduling using fixed-point integer arithmetic (`SCALE = 65,536 = 2^16`), eliminating kernel floating-point usage and FPU register traps.
- **POSIX-Lite Compatibility Layer:**
  - System call dispatcher invoked from EL0 via `SVC #5` with syscall ID in register `x8`.
  - Implements: `write()` (1), `read()` (2), `close()` (3), `getpid()` (4), `clock_gettime()` (5), and `nanosleep()` (6).
  - Extensible file descriptor table with slots 0, 1, and 2 bound to the PL011 UART console.
  - Kernel pointer boundary shield rejecting any user-supplied pointers targeting supervisor memory (`0x40000000 - 0x401fffff`) with `-EFAULT`.
- **Safe Rust Runtime SDK:**
  - User-space SDK in `runtime/rust/` providing strongly typed `TaskId` handles, capability wrappers, and safe POSIX-lite bindings.
  - Zero dynamic heap allocation (`#![no_std]` compatible).
- **C Runtime Support:**
  - Low-level service dispatch wrappers, client-side invocation helpers, and shared communication buffer utilities (`runtime/c/`).

---

## 4. Repository Structure

```text
.
├── kernel/
│   ├── arch/arm64/           # Vector table, exception handling, MMU, GICv2, timer, PAC, Elevator
│   ├── include/              # Kernel headers (arch, task, scheduler, capabilities, protection)
│   ├── scheduler/            # Decoupled scheduler core and policies (scheduler.c, rr.c, eevdf.c)
│   └── main/                 # Kernel initialization, dispatch, capability table, task management
├── runtime/
│   ├── c/                    # C client runtime (service stubs, same/cross layer call helpers)
│   ├── posix/                # POSIX-lite syscall handlers and file descriptor table
│   └── rust/                 # Safe Rust user-space SDK and POSIX-lite bindings
├── ipc/                      # IPC validation and call logic (same-layer, cross-layer, elevator)
├── memory/                   # Fixed-block allocator, shared communication buffer
├── shared/                   # Hardened DynamicArray container utility
├── benchmarks/               # Host and bare-metal microbenchmark suites
├── tests/unit/               # 11 host unit and security test suites
├── scripts/                  # Build, test, benchmark, and QEMU execution scripts
├── results/                  # Machine-readable benchmark outputs and execution logs
├── docs/                     # Detailed architectural, security, and subsystem documentation
├── .agents/                  # Autonomous agent instructions and engineering rules
├── Makefile                  # Primary workflow driver
├── CMakeLists.txt            # Host build configuration
├── AGENTS.md                 # Canonical engineering rules and architectural invariants
├── CONTRIBUTING.md           # Contribution guidelines and submission checklist
├── LICENSE                   # Apache License 2.0
└── Cargo.toml                # Rust runtime package manifest
```

---

## 5. Build Prerequisites

Lettuce requires a 64-bit Linux development environment (Ubuntu 22.04 / 24.04 LTS or Debian 12 recommended).

Install the required host toolchain, cross-compiler, QEMU emulator, and Rust toolchain:

```bash
sudo apt update && sudo apt install -y \
    build-essential \
    cmake \
    clang \
    lld \
    llvm \
    qemu-system-aarch64 \
    cargo \
    rustc \
    git
```

Verify your toolchain:
```bash
clang --version
qemu-system-aarch64 --version
cmake --version
cargo --version
```

---

## 6. Host Build & Test Commands

The host build compiles the architecture-independent microkernel logic, capability engine, task table, scheduler policies, and POSIX-lite subsystem directly for the host architecture:

```bash
# Configure the build directory
make configure

# Build all host test suites and benchmark executables
make build

# Run all 11 host unit test suites and the bare-metal ARM64 QEMU test suite
make test

# Run complete verification (unit tests, bare-metal QEMU tests, cargo check, git diff check)
make check
```

The host test suite validates:
- `capability_unit` & `capability_security`: $O(1)$ capability lookup, permission bitmask enforcement, revocation.
- `same_layer_unit` & `cross_layer_unit`: Lateral and vertical mediated IPC transitions.
- `elevator_unit`: Capability-gated critical path authorization.
- `memory_unit`: Fixed-block allocator and shared communication buffers.
- `context_nested_unit`: Nested execution contexts and state preservation.
- `dynamic_array_unit`: Bounds-checked array container primitives.
- `task_scheduler_unit`: Round-Robin baseline rotation, sleep/wakeup state transitions.
- `sched_eevdf_unit`: EEVDF virtual deadline selection, lag bounds, weighted proportional fairness.
- `posix_unit`: POSIX-lite syscalls (`read`, `write`, `close`, `getpid`, `clock_gettime`, `nanosleep`).

---

## 7. ARM64 / QEMU Build & Run Commands

The freestanding ARM64 microkernel is cross-compiled using Clang/LLD with freestanding flags (`-ffreestanding -fno-stack-protector -fno-pic -mgeneral-regs-only -nostdlib`):

```bash
# Cross-compile the freestanding ARM64 kernel image
bash scripts/build-arm64.sh
```

This generates the bare-metal ELF binary at `build-arm64/lettuce-arm64.elf`.

Boot the binary under QEMU:
```bash
bash scripts/run-qemu.sh
```

QEMU command executed under the hood:
```bash
qemu-system-aarch64 \
    -accel tcg \
    -M virt \
    -cpu max \
    -m 128M \
    -nographic \
    -monitor none \
    -serial stdio \
    -kernel build-arm64/lettuce-arm64.elf
```

The boot sequence runs **25 bare-metal execution and runtime foundation tests** covering:
- EL1/EL0 privilege transitions (`SVC` / `ERET`)
- MMU page table creation and ASID domain switching
- Memory boundary isolation and translation fault traps
- PAC continuation signing and corrupted continuation fault injection
- GICv2 interrupt routing and Generic Virtual Timer preemption
- Preemptive task switching across multiple protection domains
- POSIX-lite syscall roundtrips and supervisor memory address shields

Expected output on successful boot:
```text
All 25 ARM64 Execution/Runtime Foundation Tests Passed!
EXECUTION RUNTIME FOUNDATION PASS
```

> **Testing Environment Notice:** ARM64 execution and testing currently run under **QEMU TCG software emulation** (`-cpu max`). Timing and cycle counts collected via the ARM Generic Virtual Counter (`CNTVCT_EL0`) reflect virtual emulator ticks rather than physical silicon cycle times.
>
> **CI Measurement Notice:** GitHub Actions runners are shared automation environments. Any timing output produced there is regression-oriented only and is not publication-quality scientific performance evidence.

---

## 8. Benchmark Commands

Run the comprehensive benchmark suites:

```bash
# Run host and bare-metal benchmarks, capturing machine-readable CSVs in results/raw/
make bench
```

Alternatively, run the human-readable host benchmark runner directly:
```bash
bash scripts/bench.sh
```

The benchmark suite measures:
- Direct C function call baseline latency
- Independent $O(1)$ capability check overhead
- Same-Layer, Cross-Layer, and Elevator mediated call latency
- Round-Robin vs. EEVDF scheduler decision overhead (`pick_next`) across 2, 4, 8, and 16 active tasks
- Scheduler tick accounting latency
- Controlled fairness and weighted proportional allocation ratios (1:1, 2:1, 4:1)

Raw CSV results are captured in `results/raw/host/` and `results/raw/arm64/`.

## Evaluation Environment

The original local host measurements in `results/raw/host/` were collected on
a local development machine: **11th Gen Intel(R) Core(TM) i5-1145G7 @ 2.60GHz** (`x86_64`).
They measure the native host model and must not be read as ARM64 measurements.

The freestanding ARM64 execution path is validated under **QEMU TCG** across five host environments
(`local-intel-i5-1145g7`, `github-ubuntu-x86_64`, `github-macos-x86_64`, `github-ubuntu-arm64`, and `github-macos-arm64`)
using an identical shared ARM64 guest ELF image (`build-arm64/lettuce-arm64.elf`, SHA-256: `39c6c5514ef75421abf2c88362deef25f6d76a69ee82cc7474c7202bdbacc824`)
and the same machine parameters (`-accel tcg -M virt -cpu max -m 128M`).
All five environments execute and pass the complete 25/25 ARM64 runtime test suite and capture benchmark cases A–K.

QEMU emulator versions vary across hosts (QEMU 10.2.1 on Debian/Ubuntu local host, QEMU 8.2.2 on Ubuntu runners, and QEMU 11.1.x on macOS runners via Homebrew).
Therefore, this evaluation establishes cross-host reproducibility and provides exploratory, emulator-relative timing measurements;
numerical differences must not be attributed solely to host ISA or CPU, and counter ticks under QEMU TCG do not represent physical ARM64 silicon latency.

---

## 9. Languages & Technology Stack

| Layer | Language / Standard | Responsibilities & Architectural Scope |
|---|---|---|
| **Hardware Primitives** | **ARM64 Assembly (.S)** | Exception vectors (`VBAR_EL1`), context save/restore, `pacia`/`autia` gates, Elevator assembly gate (`elevator.S`), `DAIF` interrupt masking. |
| **Microkernel Core** | **C (C99 / C11)** | MMU page table setup, GICv2 driver, virtual timer, scheduler mechanism & policies (`rr.c`, `eevdf.c`), capability table, POSIX-lite syscalls. |
| **User Runtime SDK** | **Safe Rust (.rs)** | Type-safe capability handles, generational `TaskId` abstractions, ergonomic POSIX-lite wrappers (`runtime/rust/`). |
| **C++ Policy** | **Excluded from Core** | C++ is intentionally excluded from the privileged core to keep runtime behavior, allocation policy, exceptions, and ABI boundaries explicit. External user-space libraries may link via the C ABI. |

---

## 10. Limitations & Current Scope

- **Uniprocessor Execution:** Lettuce currently targets a single CPU core. Inter-Processor Interrupts (IPI), multi-core runqueues, and cross-core TLB shootdowns are not implemented.
- **Evaluation Scope:** Original host measurements come from a local Intel Core i5-1145G7 (`x86_64`) development machine. ARM64 execution is verified under QEMU TCG across five host environments; it establishes emulator-relative reproducibility rather than physical ARM64 silicon performance.
- **Static Task Table:** The task table supports a fixed maximum of 16 concurrent task descriptors (`LETTUCE_MAX_TASKS = 16`).
- **Minimal POSIX Subset:** POSIX-lite implements only `write`, `read`, `close`, `getpid`, `clock_gettime`, and `nanosleep`. Monolithic abstractions (`fork`, `exec`, `mmap`, `signals`, pthreads, network sockets) are deliberately omitted.
- **Hardware Security Extension Probing:**
  - **Permission Overlay Extension (POE):** Probed as unsupported on the target CPU model; Lettuce cleanly logs this and falls back to MMU isolation without fabricating software emulators.
  - **Memory Tagging Extension (MTE):** Probed as present in ISA registers, but physical tag allocation RAM is unbacked in standard QEMU virt DRAM, making it optional.

---

## 11. Contributing

Contributions that adhere to the project's architectural principles and invariant checklist are welcome.

### Key Invariants to Maintain
1. **Zero Dynamic Allocation in Hot Paths:** Never introduce `malloc`, `free`, `realloc`, or `Vec` into interrupt handlers, context switching, scheduling loops, or capability dispatch.
2. **Strict Privilege Isolation:** Microkernel code executes strictly at **EL1**; user services and synthetic tasks execute strictly at **EL0**.
3. **Authoritative Caller Identity:** Never trust user-supplied registers for caller ID; always resolve authoritatively from supervisor context.
4. **Architectural Barriers:** Every write to `TTBR0_EL1` must be preceded by `dsb ish` and followed by `isb`.
5. **No Monolithic Bloat:** Do not add monolithic OS abstractions unless mandated by the core research scope.

Please review [CONTRIBUTING.md](CONTRIBUTING.md) and [AGENTS.md](AGENTS.md) before submitting code.

---

## 12. License

Lettuce is licensed under the [Apache License, Version 2.0](LICENSE).

---

## 13. Author & Contact

- **Author:** Abdelhalim Yasser Abdelhalim
- **GitHub:** [@abdelhalimyasser](https://github.com/abdelhalimyasser)
- **Repository:** [abdelhalimyasser/LettuceOs](https://github.com/abdelhalimyasser/LettuceOs)
