# Isolation and Security Boundaries

Lettuce uses separate mechanisms for separate properties:

- Capabilities authorize a requested service operation.
- MMU page tables isolate protection-domain memory on ARM64.
- ASIDs tag TLB entries for address spaces.
- EL0/EL1 separates unprivileged services from the supervisor.
- PAC hardens saved continuation state.

[`capability.c`](../kernel/main/capability.c) stores a bounded flat table.
Handles encode a slot and generation; validation checks handle generation,
target, operation, resource, and required permission bits. Revocation clears
the entry and advances its generation. Caller identity is obtained from
kernel-owned execution state, never from user registers.

The ARM64 backend supplies per-domain roots and non-global user mappings.
The QEMU runtime suite records 25 outcomes in
[`qemu-tests.csv`](../results/raw/arm64/qemu-tests.csv), including exercised
MMU, capability, ASID, PAC, and supervisor-pointer boundary behavior. Those
tests establish behavior under QEMU TCG, not a formal proof or physical-silicon
timing result.

PAC is not capability authorization and does not isolate memory. MTE and POE
are feature probes only; the prototype makes no claim of active tag-backed MTE
protection or implemented POE overlays.
