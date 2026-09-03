---
name: research-and-docs
description: Teaches public documentation integrity, result provenance, and empirical measurement discipline.
---

# Skill: Documentation Integrity & Measurement Discipline

This skill defines public-documentation, result-provenance, and research-claim standards for Lettuce.

---

## 1. Principles of Scientific Integrity

1. **Distinguish Implementation Fact from Hypothesis:**
   - *Fact:* "Lettuce configures TTBR0_EL1 with 16-bit ASIDs and Non-Global descriptors; 25 ARM64 runtime tests execute under QEMU TCG."
   - *Hypothesis:* "ASID-tagged switching may improve macro-benchmark application throughput by preserving L1/L2 TLB locality."
2. **Never Silently Rewrite Results:**
   - Raw performance logs and benchmark outputs in [`results/`](../../../results/) represent frozen empirical evidence. Never edit them to fit a hypothesis.
3. **Transparent Limitations:**
   - Always state that ARM64 measurements were gathered on QEMU `virt` (TCG), not physical silicon. Generic Counter values are emulator-relative ticks, not CPU cycles.
   - Treat GitHub Actions timing output as regression-oriented automation data, never as publication-quality scientific performance evidence.
   - Acknowledge system limitations (e.g., uniprocessor only, POSIX-lite subset, no persistent file system).
4. **No Unsupported Novelty Claims:**
   - Position Lettuce accurately alongside established systems (seL4, Fuchsia/Zircon, L4/Fiasco.OC) rather than claiming novelty for standard OS mechanisms.

---

## 2. Documentation Architecture

Lettuce maintains a streamlined, professional documentation set in [`docs/`](../../../docs/):
- `architecture.md`: System design and layer classification.
- `arm64.md`: Low-level CPU, MMU, and vector implementation.
- `isolation-and-security.md`: Multi-tier isolation and security tests.
- `interrupts-and-scheduler.md`: GICv2, timers, and Round-Robin / EEVDF scheduler policies.
- `communication-paths.md`: Same-Layer, Cross-Layer, Elevator.
- `memory.md`: Paging, fixed allocator, and zero-allocation discipline.
- `posix-lite.md`: Implemented system call interfaces.
- `rust-runtime.md`: Safe Rust wrappers and language boundaries.
- `performance.md`: Empirical host and ARM64 benchmark results.
- `limitations.md`: Complete list of architectural and prototype limitations.
- `reproduce.md`: Step-by-step reproduction instructions.

---

## 3. Synchronizing Public Documentation and Results

When updating the codebase:
1. Verify that changes to public ABIs or syscall numbers are reflected in the relevant public documentation, including [`docs/posix-lite.md`](../../../docs/posix-lite.md).
2. If new benchmarks are run, record raw runs into `results/` before updating public summaries.
3. Keep public architecture, security, limitations, and reproduction documentation consistent with the implementation and current results.
