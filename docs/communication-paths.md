# Mediated Inter-Service Communication

Lettuce classifies services by layer; layers are **not** mandatory routing
hops. Every implemented service call is mediated by EL1, which derives caller
identity from kernel execution state rather than user registers.

```text
EL0 request -> SVC -> authoritative caller -> service lookup
            -> capability validation -> route resolution
            -> conditional domain handoff -> target EL0 -> SVC return
```

## Same-Layer

[`ipc/same_layer/validate.c`](../ipc/same_layer/validate.c) requires active
caller and target services with the same `LettuceLayer`, valid operation and
resource identifiers, and `LETTUCE_CAP_CALL`. The gate in
[`ipc/same_layer/gate.c`](../ipc/same_layer/gate.c) enters the resolved target
context and restores the prior context after the target returns.

Same layer does not mean unrestricted direct calls. Layer and memory domain are
different attributes, so a target can require a domain handoff even when both
services share a layer.

## Cross-Layer

[`ipc/cross_layer/call.c`](../ipc/cross_layer/call.c) requires different caller
and target layers plus `LETTUCE_CAP_CALL`. It resolves the target directly;
there is no required L3-to-L2-to-L1 traversal. The gate uses the same saved
context/restore structure as Same-Layer mediation.

## Elevator

Elevator policy in [`ipc/elevator/policy.c`](../ipc/elevator/policy.c) resolves
the authoritative caller and target and requires both `LETTUCE_CAP_CALL` and
`LETTUCE_CAP_CRITICAL`. It does not grant access to a layer or to EL1.

On ARM64, the exception handler may select the specialized mechanism in
[`kernel/arch/arm64/elevator.S`](../kernel/arch/arm64/elevator.S) after C policy
has succeeded. The assembly path updates the prepared target state and uses
the required barriers around `TTBR0_EL1`; it never decides authorization.
The C reference and assembly mechanism are selectable in the ARM64 test
harness, but neither is labelled universally faster.
