# Isolation & Architectural Security Model

## 1. Security Architecture Taxonomy

Lettuce enforces defense-in-depth by strictly separating security responsibilities across distinct hardware and software mechanisms. These mechanisms are orthogonal and must never be conflated:

| Security Dimension | Mechanism | Responsibility & Scope | Failure Mode / Enforcement |
|---|---|---|---|
| **Authorization** | Flat Capability Table | Decides whether Service $A$ has the right to invoke Service $B$ | `LETTUCE_STATUS_PERMISSION_DENIED` |
| **Domain Isolation** | ARM64 MMU & ASIDs | Prevents Service $A$ from reading/writing memory of Service $B$ | Hardware Translation Fault (`EC = 0x24`, `FSC = 0x07`) |
| **Privilege Isolation**| Exception Levels (EL0/EL1)| Prevents services from executing supervisor instructions | Hardware Undefined / SysReg Trap (`EC = 0x00`) |
| **Continuation Integrity**| ARMv8.3-A PAC | Protects call-gate return continuations against tampering | Hardware PAC Trap (`EC = 0x1c`) |
| **Memory Tagging** | ARMv8.5-A MTE | Optional fine-grained intra-domain memory safety (Class D) | Tag check fault (Synchronous/Asynchronous) |
| **Permission Overlay** | ARMv8.9-A POE | Optional register-indexed permission acceleration (Class C)| Permission overlay fault |

---

## 2. Layer 1: Capability-Based Authorization

All service invocations require valid capability handles. Capabilities are stored in an $O(1)$ flat table managed exclusively by the EL1 supervisor:
- **Bitmask Permissions:** `LETTUCE_CAP_READ` (`0x1`), `LETTUCE_CAP_WRITE` (`0x2`), `LETTUCE_CAP_CALL` (`0x4`), `LETTUCE_CAP_CRITICAL` (`0x8`).
- **Cryptographic Binding:** Each capability entry securely pairs the authorized `subject_service_id`, `object_service_id`, and permitted operations.
- **Fast Revocation:** Capabilities can be revoked instantaneously in $O(1)$ time by clearing the entry in the table without cascade traversals.

---

## 3. Layer 2: MMU-Backed Protection Domains

Lettuce establishes physical memory isolation via dedicated ARM64 page translation tables:
- **Kernel Space (`0x40000000 - 0x401fffff`):** Mapped as Normal Memory, Read/Write at EL1, Unprivileged Execute Never (`UXN`), Global (`nG = 0`). EL0 code has zero mapping for supervisor RAM.
- **Domain-Private Spaces:**
  - `Domain A (Camera)`: `0x41000000 - 0x410fffff`
  - `Domain B (Display)`: `0x41100000 - 0x4117ffff`
  - `Domain C (Sensor)`: `0x41180000 - 0x411bffff`
  Each domain's private pages are mapped strictly with Non-Global descriptors (`nG = 1`) tied to that domain's ASID. A service accessing another domain's private address immediately triggers a Level 2 Translation Fault.
- **Shared Communication Page (`0x411c0000 - 0x411c0fff`):** Explicitly mapped as Read/Write user memory across authorized domains to enable zero-copy parameter passing.
- **Read-Only Test Page (`0x411e0000 - 0x411e0fff`):** Mapped with AP=`0b11` (Read-Only user). Any EL0 write raises a Permission Fault (`EC = 0x24`, `FSC = 0x0e`).

---

## 4. Layer 3: Privilege Isolation (EL0 vs. EL1)

Services execute exclusively at EL0:
1. **System Registers:** Attempts to read or write privileged registers (such as `TTBR0_EL1`, `TCR_EL1`, `SCTLR_EL1`, or `VBAR_EL1`) are trapped by the CPU core and vectored to `vector_sync_lower_el` with `EC = 0x00`.
2. **Supervisor Memory Denial:** Pointers passed from EL0 into POSIX-lite syscalls or IPC parameters targeting `0x40000000 - 0x401fffff` are explicitly rejected with `-EFAULT`.

---

## 5. Layer 4: Pointer Authentication (PAC) Continuation Hardening

When services execute nested protected calls ($A \to B \to C$), the supervisor pushes the suspended execution state onto `g_context_stack`. To guarantee that an attacker compromising a domain cannot hijack return execution:
- The return address (`ELR_EL1`) is cryptographically signed using `pacia` with the kernel's private `APIAKeyLo/Hi` and the stack pointer modifier.
- When the target completes execution and returns via `SVC #0`, the kernel pops the frame and verifies the pointer using `autia`.
- If memory corruption or return-oriented programming (ROP) altered the pointer, `autia` corrupts the upper bits, and the subsequent instruction fetch triggers an immediate PAC Trap (`EC = 0x1c`), preventing control-flow hijacking.

---

## 6. Optional Architectural Features: MTE & POE

In accordance with strict research integrity:
- **FEAT_MTE (Memory Tagging):** Evaluated as **Class D (Optional)**. In QEMU `virt`, MTE instructions (`stg`, `ldg`) execute without faulting, but default DRAM lacks physical tag storage. Lettuce reports MTE instructions present but does not claim hardware-backed tag isolation.
- **FEAT_POE / FEAT_S1POE (Permission Overlay):** Evaluated as **Class C (Unsupported in QEMU virt)**. Lettuce accurately reads `ID_AA64MMFR3_EL1[39:36] == 0` and executes a clean fallback to its verified ASID-tagged MMU domain backend.

---

## 7. Verified Security Test Matrix

The following table summarizes the runtime-verified security regression suite running on ARM64 bare-metal QEMU:

| Test ID | Objective | Expected Fault / Behavior | Verified Output |
|---|---|---|---|
| **Test 5** | Verify own domain private page access | Normal EL0 Read/Write | `[PASS] Services read/write private pages at EL0` |
| **Test 6** | Foreign private page access from EL0 | Translation Fault (`EC=0x24`, `FSC=0x07`) | `[PASS] MMU translation fault trapped` |
| **Test 7** | Direct EL0 access to supervisor RAM | Permission Fault (`EC=0x24`, `FSC=0x0e`) | `[PASS] EL0 access to kernel RAM prevented` |
| **Test 8** | Direct EL0 write to privileged sysreg | Trap Exception (`EC=0x00`) | `[PASS] Privileged register access trapped` |
| **Test 13**| Stale-TLB cross-domain probing | Bidirectional translation fault | `[PASS] Stale-TLB isolation verified across ASIDs`|
| **Test 15**| Forged capability invocation | Kernel rejects call before domain switch | `[PASS] Invalid cap rejected; caller domain preserved`|
| **Test 18**| Return address continuation corruption | Hardware PAC Trap (`EC=0x1c`) | `[PASS] Corrupted continuation trapped (EC=0x1c)` |
| **Test 24**| POSIX syscall with kernel buffer pointer| Sycall rejects with `-EFAULT` | `[PASS] Kernel pointer boundary rejection verified` |
