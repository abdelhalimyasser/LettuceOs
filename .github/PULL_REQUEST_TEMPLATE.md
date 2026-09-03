## Description

<!-- Provide a brief description of the change, architectural rationale, and impacted subsystems. -->

---

## Architectural & Security Invariant Checklist

Please verify before requesting review:

- [ ] **Host Tests:** `make test` (all 11 host suites) and `make check` pass with zero failures.
- [ ] **ARM64 Bare-Metal Boot:** `bash scripts/build-arm64.sh && bash scripts/run-qemu.sh` boots and all 25 tests pass.
- [ ] **Zero Hot-Path Allocation:** Confirmed zero dynamic allocations in IRQ entry, context switch, scheduler, dispatch, or Elevator.
- [ ] **Security Invariants Preserved:** Capability checks precede domain transitions; MMU/ASID barriers and isolation remain intact; PAC continuations remain signed and authenticated.
- [ ] **Caller Identity:** Caller identity remains resolved authoritatively from kernel state, never trusted from user registers.
- [ ] **Public ABI:** Any changes to `SVC` system call numbers, descriptors, or capability bitmasks are documented in `docs/posix-lite.md`.
- [ ] **Documentation Updated:** Canonical documents in `docs/` and `.agents/` *if needed* updated if architectural behavior changed.
- [ ] **Measurement Honesty:** QEMU TCG Generic Counter ticks are described as emulator-relative, and no GitHub Actions timing output is presented as publication-quality evidence.
