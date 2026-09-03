---
name: testing-and-qemu
description: Guides host testing, bare-metal ARM64 cross-compilation, QEMU runner workflows, and CI smoke benchmarking.
---

# Skill: Testing Workflows & QEMU Emulation

This skill defines the canonical testing, cross-compilation, and QEMU execution workflows for Lettuce.

---

## 1. Host Testing Workflow

Host tests validate the platform-independent capability system, task state machine, Round-Robin and EEVDF scheduler logic, fixed allocator, and POSIX-lite interfaces:

```bash
# Clean configuration and build
cmake -S . -B build
cmake --build build --parallel

# Run all 11 host unit and security test suites
make test

# Run code hygiene, Rust checks, and git diff checks
make check
```

---

## 2. ARM64 Cross-Build Workflow

The bare-metal kernel is compiled using Clang and LLD targeting freestanding `aarch64-unknown-none-elf`:

```bash
bash scripts/build-arm64.sh
```

**Output Artifact:** `build-arm64/lettuce-arm64.elf`

---

## 3. Bare-Metal QEMU Execution Workflow

The freestanding ELF is booted under QEMU:

```bash
bash scripts/run-qemu.sh
```

### Automation & Success Detection
- **Execution Command:**
  ```bash
  qemu-system-aarch64 -M virt -cpu max -m 128M -nographic \
      -kernel build-arm64/lettuce-arm64.elf
  ```
- **Success Criteria:** The runner monitors serial console output. Execution is successful if and only if the final banner appears:
  ```text
  All 25 ARM64 Execution/Runtime Foundation Tests Passed!
  EXECUTION RUNTIME FOUNDATION PASS
  ```
- **Timeout Protection:** The runner includes a 5-second automatic timeout after the halt loop (`wfe`) is reached.

The 25 ARM64 runtime tests execute under QEMU TCG. Generic Counter values are emulator-relative ticks, not physical-silicon CPU cycles.

---

## 4. Benchmark Guidelines

- **Smoke Benchmarks (CI):** Intended solely to catch catastrophic regressions (>5x slowdowns) caused by accidental busy-loops or unbounded allocations.
- **Scientific Benchmarks (Publication):** Must be gathered under controlled conditions with cold cache warmup, recorded percentiles (p50, p95, p99), and standard deviations. GitHub Actions runners are shared and noisy; **never** present GitHub Actions CI latency numbers as publication-quality research data.
