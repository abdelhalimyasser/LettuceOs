# Prototype Scope & Known Limitations

## 1. Experimental Environment Limitations

- **Evidence Classes:** Original host measurements were collected on a local Intel Core i5-1145G7 development machine (`x86_64`). ARM64 architectural execution was evaluated on QEMU `virt` using the Tiny Code Generator (TCG) across five host environments (`local-intel-i5-1145g7`, `github-ubuntu-x86_64`, `github-macos-x86_64`, `github-ubuntu-arm64`, and `github-macos-arm64`), whose counter values do not reproduce physical silicon clocks, caches, pipelines, branch prediction, or realistic TLB timing.
- **No Physical ARM64 Evaluation:** Physical ARM64 hardware was not available for the present evaluation. GitHub-hosted runner jobs provide hosted portability and reproducibility evidence across divergent QEMU versions, not physical ARM hardware measurements or a controlled ARM-versus-x86 comparison.

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

## 4. Future Evaluation

Future work should validate the same workloads on physical ARM64 hardware with
repeated distributions, hardware-counter analysis, real TLB/ASID behavior, and
power and thermal measurements where relevant. Such work is not represented by
the current local, QEMU, or GitHub-hosted evidence.
