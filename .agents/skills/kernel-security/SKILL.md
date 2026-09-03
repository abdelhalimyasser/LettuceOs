---
name: kernel-security
description: Teaches capability validation, memory isolation, PAC continuation verification, generational handles, and security regressions.
---

# Skill: Kernel Security & Threat Containment

This skill instructs agents on enforcing Lettuce's defense-in-depth security model and running required security regression tests.

---

## 1. Security Invariants & Threat Boundaries

Agents must ensure that no modification degrades any tier of the security model:

1. **Capabilities (Authorization):**
   - Flat $O(1)$ table verification before all domain transitions.
   - Operations must match bitmask permissions (`READ`, `WRITE`, `CALL`, `CRITICAL`).
   - Revocation must execute immediately in $O(1)$ without leaving dangling references.
2. **MMU Translation (Domain Isolation):**
   - Services must never possess page table entries mapping another domain's private pages.
   - Kernel memory (`0x40000000 - 0x401fffff`) must remain inaccessible to EL0 (marked `UXN`, EL1-only).
3. **Continuation Security (PAC):**
   - Return addresses on the kernel context stack must be signed with `pacia` and authenticated with `autia`.
   - Corrupted pointers must synchronously trigger a PAC Trap (`EC = 0x1c`).
4. **Generational Safety:**
   - Memory chunks and task handles encode generation counters. Stale handles must fail in $O(1)$ time upon access.
5. **System Call Boundary:**
   - Pointer arguments to `SVC #5` must be validated. Any pointer in supervisor RAM must return `-EFAULT`.

---

## 2. Authoritative Specifications
- Security Model: [`docs/isolation-and-security.md`](../../../docs/isolation-and-security.md)
- Memory Model: [`docs/memory.md`](../../../docs/memory.md)

---

## 3. Mandatory Security Regression Testing

Whenever security-relevant logic is touched, run both host security suites and ARM64 bare-metal security tests:

```bash
# 1. Host Security Tests
./build/capability_security

# 2. Bare-Metal ARM64 Security Matrix (via QEMU)
bash scripts/build-arm64.sh && bash scripts/run-qemu.sh
```

Verify that all security tests pass:
- **Test 6:** Foreign domain access raises MMU Translation Fault (`EC=0x24`, `FSC=0x07`).
- **Test 7:** EL0 access to kernel memory raises Permission Fault (`EC=0x24`, `FSC=0x0e`).
- **Test 8:** EL0 access to privileged registers traps with Undefined/SysReg Trap (`EC=0x00`).
- **Test 13:** Stale-TLB isolation verified bidirectionally across ASIDs.
- **Test 15:** Invalid/forged capability rejected before domain switch.
- **Test 18:** PAC continuation signature verification and trap on corruption (`EC=0x1c`).
- **Test 24:** POSIX-lite syscall rejects supervisor RAM pointers with `-EFAULT`.
