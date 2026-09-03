# ARM64 Execution and Isolation

## EL0 and EL1

The ARM64 boot entry in [`boot.S`](../kernel/arch/arm64/boot.S) establishes an
early stack and transfers to EL1 C initialization. EL0 services enter through
`SVC`; [`exception.S`](../kernel/arch/arm64/exception.S) saves a trap frame and
calls EL1 handlers in [`cpu.c`](../kernel/arch/arm64/cpu.c). Return state is
prepared in `ELR_EL1`, `SPSR_EL1`, and `SP_EL0`, then resumed with `ERET`.

The fixed `LettuceTrapFrame` is shared by assembly and C. `SVC #1`, `#2`, and
`#3` select Same-Layer, Cross-Layer, and Elevator mediation; `SVC #5` selects
POSIX-lite, with its syscall ID in `x8`.

## MMU, roots, and ASIDs

[`mmu.c`](../kernel/arch/arm64/mmu.c) builds ARM64 translation tables and
switches `TTBR0_EL1` using a root plus a 16-bit ASID. User mappings are
non-global, so steady-state switches do not require a global TLBI. The switch
uses `dsb ish`, writes `TTBR0_EL1`, then uses `isb`; explicit per-ASID and
global invalidation helpers remain available when needed.

An ASID tags TLB entries; it is not an authorization primitive. Page-table
roots provide address mappings; capabilities provide call authorization.

## Interrupts, PAC, and optional features

[`gic.c`](../kernel/arch/arm64/gic.c) handles GICv2 acknowledgement and EOI.
[`timer.c`](../kernel/arch/arm64/timer.c) programs the ARM Generic Timer, whose
IRQ drives scheduler ticks. [`pac.S`](../kernel/arch/arm64/pac.S) provides
continuation signing/authentication helpers; PAC hardens control-flow state and
does not authorize calls or isolate memory.

MTE and POE code probes architectural feature exposure only. The prototype
does not claim active tag-backed MTE isolation or a POE software substitute.

## Execution environment

The ARM64 runtime tests run under QEMU TCG. Generic Counter measurements are
emulator-relative ticks, not physical CPU cycles or silicon latency.
