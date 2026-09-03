# Memory Management & Allocation Architecture

## 1. Architectural Memory Layout & Paging Model

Lettuce enforces memory protection using a 4 KiB translation granule with 48-bit virtual addressing:

```
0x0000000000000000 -------------------------------------------
                   [ Unmapped Null Guard Page (Faults on EL0/EL1) ]
0x0000000008000000 -------------------------------------------
                   [ GICv2 MMIO (0x08000000 - 0x081fffff) ]
                   - EL1 Device-nGnRE, Privileged Only
0x0000000009000000 -------------------------------------------
                   [ PL011 UART MMIO (0x09000000 - 0x09000fff) ]
                   - EL1 Device-nGnRE, Privileged Only
0x0000000040000000 -------------------------------------------
                   [ Supervisor RAM (0x40000000 - 0x401fffff) ]
                   - EL1 Read/Write/Execute, UXN, Global (nG=0)
                   - Completely unmapped in EL0 address space
0x0000000041000000 -------------------------------------------
                   [ Domain A Private Page (0x41000000 - 0x410fffff) ]
                   - EL0/EL1 RW, Non-Global (nG=1), ASID 100
0x0000000041100000 -------------------------------------------
                   [ Domain B Private Page (0x41100000 - 0x4117ffff) ]
                   - EL0/EL1 RW, Non-Global (nG=1), ASID 200
0x0000000041180000 -------------------------------------------
                   [ Domain C Private Page (0x41180000 - 0x411bffff) ]
                   - EL0/EL1 RW, Non-Global (nG=1), ASID 300
0x00000000411c0000 -------------------------------------------
                   [ Shared Communication Page (0x411c0000 - 0x411c0fff) ]
                   - EL0/EL1 RW, Inner Shareable across domains
0x00000000411e0000 -------------------------------------------
                   [ Read-Only Test Page (0x411e0000 - 0x411e0fff) ]
                   - EL0 Read-Only (Permission fault on write)
```

---

## 2. Allocation Discipline: Zero Allocation in Critical Hot Paths

A central predictability invariant of Lettuce is that **no dynamic memory allocation occurs in performance-critical or security-critical paths**:

| Subsystem Path | Allocation Policy | Enforcement Mechanism |
|---|---|---|
| **IRQ Entry & Exit** | Strictly Zero Allocation | Statically allocated trap frame on kernel stack |
| **Timer Interrupt** | Strictly Zero Allocation | Static register rearm, zero pointer manipulations |
| **Scheduler Tick** | Strictly Zero Allocation | Static 16-task table (`g_task_table`), static Round-Robin |
| **Context Switch** | Strictly Zero Allocation | Register save/restore via `context_switch.S` |
| **Same-Layer Call** | Strictly Zero Allocation | Static capability lookup, static context stack push |
| **Elevator Gate** | Strictly Zero Allocation | Assembly-specialized register/MMU swap (`elevator.S`)|
| **POSIX Syscalls** | Strictly Zero Allocation | Static 16-descriptor table, stack-allocated parameters |

---

## 3. Fixed Memory Allocator & Generational Handle Safety

For managed dynamic structures outside critical paths, Lettuce provides a fixed-block allocator ([`memory/allocator/fixed.c`](../memory/allocator/fixed.c)):
- **Chunk Size & Alignment:** Allocates fixed 64-byte blocks aligned to 64 bytes.
- **Generational Protection:** Handles encode an index and generation counter. When a chunk is freed, its slot generation increments. Stale or dangling handles passed into the allocator fail verification in $O(1)$ time, preventing use-after-free corruption.

---

## 4. Controlled Shared Communication Buffers

To avoid copying large bulk data (such as camera video frames or display compositions), Lettuce provides zero-copy shared buffers ([`memory/shared/buffer.c`](../memory/shared/buffer.c)):
- **Explicit Access Grants:** Buffers are explicitly granted to specific consumer domains.
- **State Machine:** Enforces lifecycles: `ALLOCATED` $\to$ `MAPPED` $\to$ `UNMAPPED` $\to$ `RELEASED`.
- **Clean Invalidation:** Revoking a grant immediately detaches the mapping from the consumer's translation entries.

---

## 5. DynamicArray Utility Hardening

The reusable `DynamicArray` container ([`shared/dynamic_array/dynamic_array.c`](../shared/dynamic_array/dynamic_array.c)) provides safe dynamic storage for management and tooling paths:
- **Capacity Overflow Protection:** Checks for arithmetic multiplication overflow during capacity expansion.
- **Out-of-Line Resizing:** Keeps cold reallocation routines off the primary lookup path.
- **Strict Isolation:** Never utilized inside interrupt handlers, capability dispatch, or scheduler preemption.

---

## 6. ASID & Hardware TLB Management

To prevent stale TLB entries from compromising cross-domain isolation:
- All user-space translation descriptors specify `nG = 1` (Non-Global).
- The CPU core tags all cached TLB translations with the 16-bit ASID installed in `TTBR0_EL1[63:48]`.
- When switching protection domains, writing the new ASID and translation root is sufficient; broad TLB invalidations (`tlbi vmalle1`) are omitted, eliminating cross-domain TLB thrashing.
- When tearing down or remapping a domain, targeted invalidation (`lettuce_mmu_invalidate_asid(domain_asid)`) safely purges only that specific domain's entries.
