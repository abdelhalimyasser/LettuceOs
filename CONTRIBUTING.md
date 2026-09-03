# Contributing to Lettuce

Thank you for your interest in contributing to the Lettuce operating system prototype. This project values architectural rigor, predictable microkernel behavior, and empirical reproducibility.

---

## 1. Development Prerequisites

Contributions must build cleanly with:
- **Host Compiler:** Clang $\ge$ 16 or GCC $\ge$ 12
- **Cross Compiler:** LLVM / Clang with LLD targeting `aarch64-unknown-none-elf`
- **Emulator:** QEMU $\ge$ 7.0 (`qemu-system-aarch64`)
- **Rust Toolchain:** Stable Rust $\ge$ 1.70 with Cargo
- **Build System:** CMake $\ge$ 3.22 and Make

---

## 2. Code Style & Architectural Boundaries

1. **Language Placement:**
   - Assembly (`.S`) strictly for low-level vector entries, DAIF operations, context switching, Elevator hardware switching, and PAC primitives.
   - C strictly for kernel logic, scheduling policy, MMU setup, capability validation, and system calls.
   - Rust strictly for user-space service SDKs and safe POSIX-lite wrappers.
   - C++ is intentionally excluded from the privileged core to keep runtime behavior, allocation policy, exceptions, and ABI boundaries explicit. External user-space libraries may link via the C ABI.
2. **Formatting:**
   - Ensure clean diffs without trailing whitespaces (`git diff --check`).
3. **No Allocation in Hot Paths:**
   - Never introduce dynamic memory allocation (`malloc`, `free`, `Vec`) into interrupt entry, context switches, scheduling loops, or capability dispatch.

---

## 3. Evidence Requirements for Low-Level Changes

Any pull request modifying:
- **MMU Page Tables / ASID Logic:** Must include evidence of translation fault tests and stale-TLB isolation checks.
- **Assembly Routines:** Must include disassembly inspection and register allocation verification.
- **Scheduler Logic:** Must verify preemption limits and sleep deadline handling across multiple protection domains.
- **Capability Subsystem:** Must demonstrate $O(1)$ lookup complexity and non-cascading revocation.
- **Public ABI / Syscalls:** Must update both C headers (`include/`) and Rust bindings (`runtime/rust/`).

---

## 4. Submission Checklist

Before submitting a Pull Request:
```bash
# 1. Verify host tests and code formatting
make check

# 2. Build and boot ARM64 QEMU bare-metal test suite
bash scripts/build-arm64.sh
bash scripts/run-qemu.sh
```
Ensure all 11 host test suites and all 25 ARM64 runtime tests pass. ARM64 output is collected under QEMU TCG: Generic Counter ticks are emulator-relative, not physical-silicon CPU cycles, and GitHub Actions timing output is not publication-quality performance evidence.
