---
name: lettuce-architecture
description: Teaches Lettuce microkernel architectural principles, layer classification, communication paths, and language boundaries.
---

# Skill: Lettuce System Architecture & Design Principles

This skill equips an agent to understand, navigate, and propose safe architectural changes to Lettuce without violating its microkernel design principles.

---

## 1. Architectural Foundations & References

- **Layer Classification:** Lettuce classifies services into $L_1$ (Core Resources), $L_2$ (Scheduling/Performance), $L_3$ (Native Services), and $L_4$ (External/Legacy).
- **Not a Mandatory Pipeline:** Services communicate laterally via Same-Layer, vertically via Cross-Layer, and time-critical downward via Elevator. Layer placement is an authorization scope, not a sequential routing hop.
- **Authoritative Specifications:**
  - System Overview: [`docs/architecture.md`](../../../docs/architecture.md)
  - Communication Topologies: [`docs/communication-paths.md`](../../../docs/communication-paths.md)
  - Canonical Instructions: [`AGENTS.md`](../../../AGENTS.md)

---

## 2. Language Placement Discipline

When designing new components or refactoring existing ones, place code strictly in the designated tier:

1. **Assembly (`kernel/arch/arm64/*.S`):** Unavoidable low-level hardware operations: vectors, DAIF masking, context swaps, Elevator hardware gate, PAC instructions.
2. **C (`kernel/`):** Kernel mechanisms, scheduling policies, GIC drivers, MMU logic, capabilities, and system call boundaries.
3. **Rust (`runtime/rust/`):** Safe service SDKs, strongly typed handles, and ergonomic POSIX-lite wrappers.
4. **C++:** Prohibited in kernel core.

---

## 3. Pre-Change Architectural Checklist

Before submitting or proposing any architectural change, verify:

- [ ] Does the change maintain strict EL1 supervisor vs. EL0 service isolation?
- [ ] Is caller identity authoritatively resolved from the kernel context, never trusted from user registers?
- [ ] Is the path strictly free of dynamic memory allocation?
- [ ] If modifying the Elevator path, does all authorization remain in C?
- [ ] Have both host (`make test`) and ARM64 bare-metal tests (`scripts/run-qemu.sh`) been executed?
