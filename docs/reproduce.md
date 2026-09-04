# Reproduction Guide & Build Instructions

This guide provides copy-pasteable instructions to build, test, and run the Lettuce research operating system from a fresh clone.

---

## 1. System Requirements & Prerequisites

### Linux (Ubuntu 22.04 / 24.04 or Debian 12)
Install the required host compilers, cross-compilers, QEMU, and build tools:

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    clang \
    lld \
    llvm \
    qemu-system-arm \
    qemu-system-aarch64 \
    cargo \
    rustc \
    git
```

Verify installations:
```bash
clang --version
qemu-system-aarch64 --version
cmake --version
cargo --version
```

---

## 2. Host Build & Verification

The host build exercises the architecture-independent capability engine, dispatcher, task model, fixed allocator, and POSIX-lite interfaces directly on your host CPU:

The tracked original host measurements were collected on the author's Intel
Core i5-1145G7 (`x86_64`) development machine. A reproduction on another
machine is a new measurement, not a replacement for the recorded evidence.

```bash
# Configure and build host executables
cmake -S . -B build
cmake --build build --parallel

# Execute all host unit and security test suites
make test

# Run complete static checks and Rust verification
make check
```

Expected Output:
```text
Running task_scheduler_unit tests...
[PASS] task_scheduler_unit tests passed!
Running posix_unit tests...
[PASS] posix_unit tests passed!
cargo check
    Finished `dev` profile [unoptimized + debuginfo] target(s) in 0.03s
```

---

## 3. Bare-Metal ARM64 Build & QEMU Execution

Build the freestanding ARM64 microkernel image (`build-arm64/lettuce-arm64.elf`) and boot it in QEMU:

```bash
# 1. Compile the bare-metal ARM64 ELF image
bash scripts/build-arm64.sh

# 2. Boot under QEMU virt and run all 25 runtime & security tests
bash scripts/run-qemu.sh
```

### Verification Criteria
The test harness will run through all 25 tests. Execution is successful if and only if the final banner appears:

```text
============================================================
All 25 ARM64 Execution/Runtime Foundation Tests Passed!
EXECUTION RUNTIME FOUNDATION PASS
============================================================
```

---

## 4. Running Benchmarks

### Host Microbenchmarks & Scheduler Evaluation
```bash
# Run all host microbenchmarks including IPC, capability, and scheduler
bash scripts/bench.sh

# Or execute the dedicated scheduler fairness and overhead suite directly:
./build/scheduler_bench

# Or run the standalone EEVDF unit test suite:
./build/sched_eevdf_unit
```

### ARM64 In-Kernel Benchmarks
The ARM64 statistical microbenchmarks (Cases A through K) run automatically as part of `bash scripts/run-qemu.sh` immediately following Test 25. They run under QEMU TCG and report emulator-relative Generic Counter ticks, not physical ARM CPU cycles.

## 5. Hosted CI Matrix

GitHub Actions also runs the host model on hosted Ubuntu and macOS x86_64/ARM64
runners. This is portability and test-reproducibility evidence. It is not a
controlled benchmark comparison and does not show that the author tested
Lettuce on physical ARM64 hardware.
