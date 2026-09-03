# Lettuce Architecture

Lettuce is an ARM64 research microkernel prototype. EL1 owns supervisor code,
page-table roots, capability metadata, exception handling, and scheduling;
synthetic services execute at EL0.

```text
EL0 service -> SVC -> EL1 identity/capability/dispatch -> target context -> ERET
```

## Classification and isolation

`L1` through `L4` are service-classification labels defined by
[`LettuceLayer`](../include/lettuce/service.h). They do not describe mandatory
routing hops. A `LettuceDomainId` names a protection domain; the ARM64 backend
associates domains with page-table roots and ASIDs. A capability authorizes a
particular target, operation, resource, and permission set. These concepts are
deliberately separate.

The public call paths are Same-Layer, Cross-Layer, and Elevator. All resolve
the caller from kernel state and validate capability rights before target
execution. See [`communication-paths.md`](communication-paths.md).

## Implementation boundaries

- [`kernel/main/`](../kernel/main/) owns portable registry, context,
  capability, dispatch, and task mechanisms.
- [`kernel/arch/arm64/`](../kernel/arch/arm64/) owns boot, vectors, MMU, GIC,
  timer, PAC primitives, and the specialized Elevator transition.
- [`kernel/scheduler/`](../kernel/scheduler/) separates common mechanism from
  Round Robin and EEVDF policy hooks.
- [`runtime/rust/`](../runtime/rust/) supplies safe EL0-facing wrappers over
  C ABI boundaries; it does not implement privileged kernel mechanisms.
