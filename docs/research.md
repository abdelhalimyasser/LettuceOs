# Research Scope and Non-Claims

Lettuce explores capability-mediated service transitions together with ARM64
exception handling, MMU domain roots, ASID tags, and PAC-hardened
continuations. Its public evidence is the current source tree and
[`results/raw/`](../results/raw/).

The prototype demonstrates implemented mechanisms and their tests. It does not
claim formal verification, production readiness, complete POSIX compatibility,
or physical ARM64 performance validation. Original host measurements were
collected locally on an Intel Core i5-1145G7 (`x86_64`); ARM64 execution uses
QEMU TCG across five host environments. Generic Counter values are emulator-relative and do not establish
silicon cycle costs. GitHub-hosted ARM64 CI jobs are portability evidence, not
physical ARM hardware measurements.

Capabilities authorize service operations. ASIDs tag TLB entries. PAC hardens
continuations. None substitutes for the others.
