---
name: arm64-low-level
description: Teaches ARM64 low-level architectural mechanics, exception levels, MMU, ASID, vectors, GICv2, timers, and assembly rules.
---

# Skill: ARM64 Low-Level Systems & Hardware Mechanics

This skill guides agents in inspecting, debugging, and authoring ARM64 architectural code across privilege boundaries.

---

## 1. Core Architectural Invariants

- **Privilege Separation:** Microkernel runs at EL1 (`SPSR_EL1.M = 0b0101`, EL1h); user services run at EL0 (`SPSR_EL1.M = 0b0000`, EL0t).
- **Stack Alignment:** The stack pointer (`sp`) must remain 16-byte aligned at all public function entries and exception boundaries (`_Alignof == 16`).
- **AAPCS64 Register Usage:**
  - `x0 - x7`: Parameter passing and return values (`x0` for primary status/result).
  - `x8`: Indirect result location / system call identifier (`LETTUCE_SYS_*`).
  - `x9 - x15`: Caller-saved scratch registers.
  - `x19 - x28`: Callee-saved registers (preserved across context switches).
  - `x29` (FP), `x30` (LR): Frame pointer and link register.
- **Authoritative Specifications:**
  - Low-Level Architecture: [`docs/arm64.md`](../../../docs/arm64.md)
  - Interrupts & Scheduling: [`docs/interrupts-and-scheduler.md`](../../../docs/interrupts-and-scheduler.md)

---

## 2. MMU & Memory Barrier Rules

1. **Non-Global Descriptors:** All user mappings must specify bit 11 (`nG = 1`) to bind entries to the active ASID in `TTBR0_EL1[63:48]`.
2. **Mandatory Barriers:**
   - Every modification of `TTBR0_EL1` must be preceded by `dsb ish` (Data Synchronization Barrier, Inner Shareable) to drain pending memory accesses.
   - Every modification of `TTBR0_EL1` must be followed by `isb` (Instruction Synchronization Barrier) to flush the instruction prefetch buffer.
3. **Hardware Truthfulness:**
   - Pointer Authentication: Supported and active (`ID_AA64ISAR1_EL1.API = 5`).
   - Memory Tagging (MTE): Instructions present, but DRAM unbacked in virt; report as optional.
   - Permission Overlay (POE): Unsupported in standard virt (`S1POE = 0`); report as unsupported.

---

## 3. Low-Level Verification Workflow

When authoring or optimizing assembly/low-level C code:
1. Inspect the compiled disassembly to verify register allocation and instruction scheduling:
   ```bash
   llvm-objdump -d build-arm64/lettuce-arm64.elf > results/disassembly/arm64-full-kernel.asm
   ```
2. Verify exception frame layout assertions:
   ```c
   _Static_assert(sizeof(LettuceTrapFrame) == 272, "LettuceTrapFrame must be 272 bytes.");
   _Static_assert(_Alignof(LettuceTrapFrame) == 8, "LettuceTrapFrame must be 8-byte aligned.");
   ```
3. Run the complete bare-metal test suite under QEMU:
   ```bash
   bash scripts/build-arm64.sh && bash scripts/run-qemu.sh
   ```
