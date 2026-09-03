# ARM64 Low-Level Architectural Foundation

## 1. Exception Level Model & Boundary Control

Lettuce strictly enforces the ARMv8-A privilege hierarchy:
- **EL1 (Supervisor Mode):** Houses the trusted microkernel core, exception vectors, page table roots, interrupt controller drivers, and capability tables.
- **EL0 (User Mode):** All synthetic services and user tasks execute strictly unprivileged. EL0 execution cannot alter translation tables, access physical device registers, or disable interrupts.

```
       EL0 (Unprivileged Services)
          |                    ^
          | SVC #0..#5         | ERET
          v                    |
       EL1 (Supervisor Microkernel Core)
```

### System Call Calling Convention (`SVC`)
Service-to-kernel requests enter EL1 via the `SVC` instruction:
- `SVC #0`: Service completion / return to caller (`x0 = status`).
- `SVC #1`: Same-Layer protected call (`x0 = cap_handle`).
- `SVC #2`: Cross-Layer protected call (`x0 = cap_handle`).
- `SVC #3`: Elevator protected call (`x0 = cap_handle`).
- `SVC #4`: Cooperative thread yield.
- `SVC #5`: POSIX-lite syscall (`x8 = syscall_id`, `x0..x5 = args`).

The kernel resumes or enters EL0 execution via the `ERET` instruction after programming `ELR_EL1` (target entry address) and `SPSR_EL1` (cleared to `0x0` targeting `EL0t`).

---

## 2. Exception Vector Architecture

The exception vector table in [`kernel/arch/arm64/exception.S`](../kernel/arch/arm64/exception.S) defines 16 entry points aligned to 128-byte boundaries, registered in `VBAR_EL1`:

| Offset | Vector Name | Trigger Condition | Handler Action |
|---|---|---|---|
| `0x200` | `vector_sync_el1` | Synchronous exception in EL1 | Traps PAC authentication failures (`EC = 0x1c`) or fatal kernel faults |
| `0x280` | `vector_irq_el1` | Asynchronous IRQ while in EL1 | Acknowledges GIC, services timer, skips task preemption |
| `0x400` | `vector_sync_lower_el` | Synchronous trap from EL0 (`SVC`, MMU Data/Instruction Abort) | Dispatches service calls, enforces MMU fault isolation |
| `0x480` | `vector_irq_lower_el` | Asynchronous IRQ while in EL0 | Saves trap frame, decrements task quantum, triggers preemption |

---

## 3. Memory Management Unit (MMU) & ASID Translation

Lettuce configures a 48-bit Virtual Addressing space using 4 KiB translation granules and 4-level translation paging:
- **TCR_EL1:** Configured with `T0SZ = 16` (48-bit VA range), `TG0 = 0b00` (4 KiB granule), `SH0 = 0b11` (Inner Shareable), `ORGN0/IRGN0 = 0b01` (Normal Memory, Write-Back Write-Allocate Cacheable).
- **MAIR_EL1:** Defines Attribute 0 as Normal Memory (`0xff`) and Attribute 1 as Device-nGnRE (`0x04`).

### ASID-Aware Address Space Switching
To eliminate expensive global TLB invalidations (`tlbi vmalle1`) on every domain switch, each protection domain is assigned an immutable 16-bit ASID:
- `DOMAIN_A`: ASID `100` (`0x0064`)
- `DOMAIN_B`: ASID `200` (`0x00c8`)
- `DOMAIN_C`: ASID `300` (`0x012c`)

All user-space pages are tagged as **Non-Global** (`nG = 1`, bit 11 in page descriptors). When switching domains:
```c
uint64_t new_ttbr0 = domain_page_table_base | ((uint64_t)domain_asid << 48);
__asm__ __volatile__(
    "dsb ish\n"
    "msr ttbr0_el1, %0\n"
    "isb\n"
    : : "r"(new_ttbr0) : "memory"
);
```
Because user TLB entries include the 16-bit ASID tag, entries from Domain A and Domain B coexist in the hardware TLB simultaneously without leaking access or requiring cache/TLB flushes.

---

## 4. Pointer Authentication (PAC)

Lettuce utilizes ARMv8.3-A Pointer Authentication (`FEAT_PACQARMA5`, `ID_AA64ISAR1_EL1.API = 0x5`) to protect kernel-side continuation pointers:
1. **Key Generation:** Supervisor initializes `APIAKeyLo_EL1` and `APIAKeyHi_EL1` with private random seeds during boot.
2. **Continuation Signing:** When an EL0 service issues a nested call, the kernel pushes the caller's execution frame onto `g_context_stack` and cryptographically signs the return address using the stack pointer as a modifier:
   ```assembly
   pacia x0, x1    /* x0 = return address, x1 = context stack pointer */
   ```
3. **Authentication:** Upon return, the address is verified:
   ```assembly
   autia x0, x1
   ```
   If an adversary tampers with the continuation pointer in memory, the authentication fails, inserting an invalid PAC error pattern into the upper address bits. Subsequent instruction fetch synchronously faults with `EC = 0x1c` (`PAC Trap`), halting the rogue execution immediately.

---

## 5. Hardware Interrupt Controller & Timer Subsystem

### GICv2 Subsystem
The ARM Generic Interrupt Controller v2 interface is memory-mapped at `0x08000000`:
- **GICD (Distributor, `0x08000000`):** Manages interrupt routing, priorities, and enable masks across the system.
- **GICC (CPU Interface, `0x08010000`):** Handles interrupt acknowledgment (`GICC_IAR`) and signaling completion (`GICC_EOIR`).

### ARM Generic Virtual Timer
The system programs the virtual timer PPI (`INTID 27`):
- Reads frequency from `CNTFRQ_EL0` (typically 62.5 MHz under QEMU virt).
- Programs countdown value into `CNTV_TVAL_EL0` for a 100 Hz quantum (10 ms).
- Enables timer and unmasks IRQ signaling via `CNTV_CTL_EL0 = 0x1`.

---

## 6. Language Implementation Boundary

Lettuce enforces a strict policy regarding implementation languages:

```
               Language Division of Responsibility
  ┌─────────────────────────────────────────────────────────┐
  │ Assembly (ARM64)                                        │
  │ - Exception vectors (exception.S)                       │
  │ - Boot strapping & initial stack setup (boot.S)         │
  │ - Callee-saved context switch (context_switch.S)        │
  │ - Specialized Elevator transition gate (elevator.S)     │
  │ - PAC signing and verification gates (pac.S)            │
  │ - DAIF interrupt masking/unmasking (irq.S)              │
  ├─────────────────────────────────────────────────────────┤
  │ C (Kernel & Infrastructure Core)                        │
  │ - MMU table construction & domain switching (mmu.c)     │
  │ - GICv2 distributor & CPU interface driver (gic.c)      │
  │ - Preemptive Round-Robin scheduler (scheduler.c)        │
  │ - Task model & state machine (task.c)                   │
  │ - Capability evaluation & revocation (capability.c)     │
  │ - Dispatcher mediation logic (dispatch.c)               │
  │ - POSIX-lite syscall layer & file descriptors (sys.c)   │
  ├─────────────────────────────────────────────────────────┤
  │ Rust (Safe Higher-Level Runtimes & Wrappers)            │
  │ - User-space service SDK & ergonomic wrappers           │
  │ - Safe POSIX-lite interfaces (stdout_write, sleep_ms)   │
  │ - Strongly typed TaskId and CapabilityHandle wrappers   │
  └─────────────────────────────────────────────────────────┘
```
