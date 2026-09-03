# Lettuce System Architecture

## 1. Overview

**Lettuce** is a research operating system prototype designed for ARM64 architectures. It investigates how modern processor security mechanisms—specifically hardware Address Space Identifiers (ASIDs), Memory Management Units (MMU), and Pointer Authentication (PAC)—can be combined with a layered capability architecture to achieve high-performance, mediated inter-service communication without relinquishing hardware fault isolation.

Lettuce enforces strict privilege separation between a minimal privileged supervisor executing at Exception Level 1 (EL1) and isolated user-space services executing at Exception Level 0 (EL0).

```
 +-----------------------------------------------------------------------+
 |                             EL0 USER SPACE                            |
 |                                                                       |
 |   +--------------------+  Same-Layer  +--------------------+          |
 |   |      Layer 3       | <----------> |      Layer 3       |          |
 |   |   Camera Service   |              |   Display Service  |          |
 |   |   (Domain A: 100)  |              |   (Domain B: 200)  |          |
 |   +---------+----------+              +--------------------+          |
 |             |                                                         |
 |             | Cross-Layer                                             |
 |             v                                                         |
 |   +--------------------+                                              |
 |   |      Layer 2       | <..................................+         |
 |   |   Sensor Service   |                                    :         |
 |   |   (Domain C: 300)  |                                    :         |
 |   +--------------------+                                    : Elevator|
 |             |                                               : (Bypass)|
 |             + - - - - - - - - - - - - - - - - - - - - - - - +         |
 +-----------------------------------+-----------------------------------+
                                     | SVC (#0, #1, #2, #3, #4, #5)
                                     v ERET
 +-----------------------------------------------------------------------+
 |                            EL1 KERNEL CORE                            |
 |                                                                       |
 |   +--------------------+   +--------------------+   +-------------+   |
 |   |   Service Table    |   |  Capability Table  |   | Dispatcher  |   |
 |   |    (Authoritative) |   |    (O(1) Flat)     |   | (Mediator)  |   |
 |   +--------------------+   +--------------------+   +-------------+   |
 |                                                                       |
 |   +--------------------+   +--------------------+   +-------------+   |
 |   |     MMU & ASID     |   |    PAC Key Mgmt    |   | GICv2/Timer |   |
 |   |  (TTBR0 Switching) |   |  (Continuation)    |   | (Scheduler) |   |
 |   +--------------------+   +--------------------+   +-------------+   |
 +-----------------------------------------------------------------------+
```

---

## 2. Layered Service Classification Model

Lettuce structures system services into four logical layers ($L_1$--$L_4$) surrounding the privileged supervisor:

- **Layer 1 ($L_1$ - Core Resources):** Hardware resource managers, physical page allocators, and interrupt routing policy.
- **Layer 2 ($L_2$ - Performance & Scheduling):** High-frequency infrastructure components, CPU scheduling policy, and task abstractions.
- **Layer 3 ($L_3$ - Native Services):** High-throughput OS services and device pipelines (e.g., Camera, Display, Storage, Audio).
- **Layer 4 ($L_4$ - External & Legacy):** Third-party extensions, sandboxed application runtimes, and untrusted legacy drivers.

### Classification vs. Routing Policy
Layer placement in Lettuce serves as a **classification and scoping model**, **not a mandatory sequential pipeline**. Services in Layer 3 are not required to route through Layer 4, nor are calls forced to traverse intermediate layers step-by-step unless governed by architectural policy. The layer metadata determines permissible communication paths, privilege ceilings, and authorization rules.

---

## 3. Separation of Concerns: Mechanism vs. Policy

A founding design principle of Lettuce is the strict decoupling of architectural mechanisms from kernel policy:

| Dimension | Architectural Mechanism | Kernel Policy | Enforcing Component |
|---|---|---|---|
| **Privilege** | ARM64 Exception Levels (`EL1` vs `EL0`), `SVC`, `ERET` | Only supervisor core runs at EL1; all services execute at EL0 | Hardware CPU & Exception Vectors |
| **Authorization** | Flat Capability Table with bitmask permissions | Capability verification required before every domain crossing | `kernel/main/capability.c` |
| **Isolation** | ARM64 MMU Translation Tables (`TTBR0_EL1`), ASIDs | Domain-private pages mapped exclusively in owning domain | `kernel/arch/arm64/mmu.c` |
| **Integrity** | ARMv8.3-A Pointer Authentication (`PACIA` / `AUTIA`) | Call-gate continuations signed with private supervisor keys | `kernel/arch/arm64/pac.S` |
| **Dispatch** | Synchronous context save/restore, Assembly Elevator gate | Same-Layer, Cross-Layer, and Elevator transition paths | `kernel/main/dispatch.c`, `elevator.S` |
| **Scheduling** | GICv2 virtual timer PPI (`#27`), preemptive trap frame swap | Policy-separated scheduling (Round-Robin baseline and integer EEVDF) | `kernel/scheduler/scheduler.c`, `rr.c`, `eevdf.c` |

---

## 4. Protected Communication Paths

Lettuce defines three distinct communication topologies for mediated service interaction:

### A. Same-Layer (Lateral) Calls
Permits lateral invocations between peer services residing within the identical layer (`caller.layer == target.layer`).
- **Use Case:** Collaboration between peer native services, such as a camera frame pipeline directly feeding a display compositor.
- **Authorization:** Standard `LETTUCE_CAP_CALL` capability checked in $O(1)$ time.

### B. Cross-Layer (Vertical) Calls
Governs controlled invocations across different layer boundaries (`caller.layer != target.layer`).
- **Use Case:** Higher-layer applications or services requesting infrastructure functions from lower-tier managers.
- **Authorization:** Validates hierarchy permissions, parameter envelopes, and `LETTUCE_CAP_CALL`.

### C. Elevator (Capability-Gated Critical Bypass) Calls
Provides a low-latency downward bypass path across multiple layers without traversing intermediate software stacks.
- **Use Case:** Time-critical, hard-deadline emergency paths (e.g., Camera $L_3$ signaling Sensor/Hardware $L_1$ to halt capture).
- **Authorization:** Requires both `LETTUCE_CAP_CALL` and `LETTUCE_CAP_CRITICAL`. All authorization remains strictly evaluated in C; an assembly-specialized transition gate (`elevator.S`) accelerates the physical MMU and register swap.

---

## 5. Kernel Core Architecture

The supervisor core consists of minimal, statically allocated subsystems:
1. **Authoritative Identity:** Kernel maintains trusted execution contexts (`current_service_id`, `current_domain_id`) that cannot be spoofed by user registers.
2. **Capability Engine:** Flat $O(1)$ table supporting creation, verification, and non-cascading revocation.
3. **Dispatch Mediator:** Mediates EL0 requests, coordinates MMU address space switches, and restores caller domains upon return.
4. **Policy-Based Task Scheduler:** Statically bounded 16-task table separating hardware context/MMU switching from pluggable policies (Round-Robin baseline and integer EEVDF with virtual deadlines and weight models).
5. **POSIX-Lite Interface:** Extensible file-descriptor table mapping standard streams (`0`, `1`, `2`) to PL011 UART and providing basic system queries.
