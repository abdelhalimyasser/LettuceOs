# Prototype Scope & Known Limitations

## 1. Experimental Environment Limitations

- **QEMU Virtualization Artifacts:** All ARM64 measurements were gathered on QEMU `virt` utilizing the Tiny Code Generator (TCG). Emulation introduces software translation artifacts, timing jitters, and instruction dispatch behaviors that do not reflect physical silicon clock cycles or cache hierarchies.
- **Absence of Physical Hardware Measurements:** The prototype has not yet been booted or measured on physical ARM64 development boards (e.g., Apple Silicon M-series or Raspberry Pi 4/5). Physical silicon pipeline stalls and cache line refills remain unmeasured.

---

## 2. Kernel & OS Subsystem Limitations

- **Research Scheduling Scope:** The scheduler implements a mechanism/policy split supporting both Round-Robin baseline and integer-arithmetic EEVDF. However, it operates on a statically bounded 16-task table, is uniprocessor-only, and does not support priority inheritance or multi-core load balancing. EEVDF weights are bounded between 1 and 10,240 using fixed-point scaling ($2^{16}$) to guarantee absence of floating point or integer overflow.
- **No File System or Block Storage:** Persistent storage drivers, block layers, and hierarchical file systems are not implemented.
- **No Networking Stack:** Network card drivers, TCP/IP stacks, and socket layers are absent.
- **POSIX-Lite Only:** Lettuce supports only `write`, `read`, `close`, `getpid`, `clock_gettime`, and `nanosleep`. Process creation (`fork`, `exec`), signals, and virtual memory remapping (`mmap`) are intentionally omitted.
- **Static Capacities:** The microkernel limits task capacity to 16 tasks and 16 file descriptors to ensure bounded $O(1)$ memory usage.

---

## 3. Hardware Feature Availability Limitations

- **Memory Tagging Extension (MTE):** In standard QEMU `virt`, MTE instructions (`stg`, `ldg`) execute without faulting, but system RAM is unbacked by physical tag storage. Intra-domain tag safety is not active by default.
- **Permission Overlay Extension (POE):** POE requires ARMv8.9-A hardware. Current QEMU virt does not implement `S1POE`, requiring Lettuce to fall back to standard ASID-tagged MMU domain isolation.
