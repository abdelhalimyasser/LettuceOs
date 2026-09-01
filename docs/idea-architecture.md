# Proposed ARM64 Layered Service Architecture

## The Basic Architecture

```txt
                 ┌──────────────────────┐
                 │       ASM + C        │
                 │                      │
                 │     MAIN KERNEL      │
                 │                      │
                 │ traps / IRQ / MMU    │
                 │ capability core      │
                 │ PAC / fast gates     │
                 └──────────┬───────────┘
                            │
                     ELEVATOR / VIP
                ASM + C capability gate
                            │
             ┌──────────────┴──────────────┐
             │                             │
     Normal L1/L2 access            Critical L3/L4
             │                             │

================================= L1 =================================

                 CORE / CRITICAL OS SERVICES
                      mainly C + ASM

    ┌──────────────────┐     fast protected     ┌──────────────────┐
    │    Service A     │ <──── local call ────> │    Service B     │
    │ isolated domain  │    token + validation  │ isolated domain  │
    └──────────────────┘                        └──────────────────┘

================================= L2 =================================

        PERFORMANCE / VIRTUALIZATION / ABSTRACTION
                     mainly C

    ┌──────────────────┐     fast protected     ┌──────────────────┐
    │    Service C     │ <──── local call ────> │    Service D     │
    │ isolated domain  │    token + validation  │ isolated domain  │
    └──────────────────┘                        └──────────────────┘

================================= L3 =================================

               NATIVE SYSTEM SERVICES
                  C / C++ / Rust

    ┌──────────────────┐     fast protected     ┌──────────────────┐
    │    Service E     │ <──── local call ────> │    Service F     │
    │ isolated domain  │    token + validation  │ isolated domain  │
    └──────────────────┘                        └──────────────────┘

================================= L4 =================================

        EXTERNAL / LEGACY / THIRD-PARTY / POSIX

             C / C++ / Rust as required
```

---

## Architecture Overview

The proposed architecture is organized as a hierarchical set of isolated service layers built around a minimal privileged kernel.

The hierarchy does not represent a mandatory communication route. A service does not need to traverse every intermediate layer in order to reach another service. Instead, the layers represent different classes of trust, criticality, latency sensitivity, resource guarantees, and expected communication behavior.

The design provides three communication mechanisms:

1. **Same-layer protected calls** for services that frequently communicate and have similar criticality and performance requirements.
2. **Cross-layer service calls** for normal communication between services located in different layers.
3. **The Elevator**, a capability-protected fast path used for direct, latency-sensitive, or critical communication with the Main Kernel or other highly critical services.

Every service remains isolated, including services located in the same layer. Placement in the same layer allows services to use a lightweight protected call mechanism, but does not imply shared trust, unrestricted memory access, or a shared failure domain.

Each service is identified by the system and receives explicit capabilities describing which services, operations, and resources it is allowed to access. Possession of a capability does not provide general access to an entire layer.

The initial implementation targets ARM64 and uses C and Assembly for the most latency-sensitive and privileged mechanisms, while higher-level and less-trusted services may use C, C++, or Rust depending on their requirements.

---

## Main Kernel

The Main Kernel contains only mechanisms that require the highest level of privilege or must remain on the shortest possible execution path.

Recommended responsibilities:

- Exception and trap entry
- Interrupt entry and dispatch primitives
- Context switching
- Protection-domain switching primitives
- MMU and page-table primitives
- Capability enforcement core
- Secure call gates
- PAC-assisted control-flow hardening for sensitive gates
- POE/POE2 permission-overlay primitives when available
- Low-level CPU control
- Low-level timer mechanisms
- Atomic and synchronization primitives
- Minimal resource-enforcement mechanisms

The Main Kernel should remain intentionally small.

Most architecture-specific fast paths may be implemented in Assembly, while control logic should remain in C where possible.

---

## L1 — Core Resource and Critical Control Services

L1 contains services whose failure, starvation, or excessive latency can directly affect the operation of the entire system.

These services should normally be:

- Small
- First-party
- Highly validated
- Performance-sensitive
- Strongly resource-guaranteed
- Frequently involved in critical system operations

Recommended initial L1 services:

- Memory resource manager
- Physical memory allocation policy
- Memory pressure and reclamation service
- Core power-management service
- Thermal-management control
- Core system resource manager
- Critical watchdog and recovery service
- Security policy service
- Critical clock/time management
- System-wide resource accounting

L1 should not contain arbitrary drivers or large compatibility code.

Low-level privileged mechanisms remain in the Main Kernel, while L1 contains the higher-level management and policy associated with those mechanisms.

---

## L2 — Performance, Scheduling, Virtualization, and Platform Services

L2 contains services that are highly performance-sensitive but are not fundamental privileged kernel mechanisms.

This layer also provides the main portability and virtualization boundary between the architecture-independent service model and platform-specific hardware.

Recommended initial L2 services:

- Scheduling policy service (policy only; privileged context-switch mechanism remains in Main)
- CPU placement and load-balancing policy
- Virtual-machine manager
- Virtual CPU management
- Platform abstraction services
- Architecture backend services
- I/O virtualization management
- IOMMU/SMMU policy and mapping management
- Accelerator management
- GPU/NPU resource coordination
- Performance-governor services
- High-performance shared-buffer management
- Device-resource arbitration

L2 is intended to allow the upper service architecture to remain largely independent from whether the system runs on a phone, laptop, or another ARM64 platform.

Architecture-specific privileged operations are delegated to the Main Kernel through controlled interfaces.

---

## L3 — Native Operating-System Services

L3 contains native first-party operating-system services that do not normally require privileged execution but still benefit from integration with the system's capability, scheduling, and fast-communication model.

Recommended initial L3 services:

- Native file-system service
- Native storage service
- Network stack
- Audio service
- Camera service
- Display/compositor service
- Sensor services
- Input service
- Bluetooth native service
- Native USB management service
- Media processing services
- Location service
- Native device-management services
- System configuration services

These services are expected to be isolated from one another.

Services that communicate frequently may be placed in the same layer and use same-layer protected calls after capability validation.

Rust is a preferred implementation language for new L3 components, while C or C++ may be used when required by existing libraries, hardware interfaces, or performance constraints.

---

## L4 — External, Legacy, Third-Party, and Compatibility Services

L4 contains components with the lowest default level of trust and the strongest containment requirements.

Recommended initial L4 components:

- Third-party drivers
- Legacy drivers
- Reused Linux drivers
- Vendor drivers that cannot be fully validated
- POSIX compatibility services
- Linux ABI/API compatibility components
- Legacy protocol stacks
- External-device services
- Optional third-party system extensions
- Compatibility runtimes
- Untrusted hardware-support components

L4 services receive the minimum capabilities required for their operation.

They should not be able to directly access the Main Kernel or critical services unless they possess an explicit capability for a narrowly defined operation.

Critical operations originating from L4 may use the Elevator only when an explicit capability authorizes the specific target and operation.

C and C++ are expected to remain necessary for legacy compatibility, while new isolation wrappers, service runtimes, and native replacement components may be implemented in Rust.

---

## Communication Model

### Same-Layer Protected Calls

Services located in the same layer may use a lightweight protected call mechanism when:

- They communicate frequently
- Their latency requirements are similar
- Their criticality is similar
- Their communication relationship is known and explicitly authorized

Conceptually:

```txt
Service A
    |
    v
lightweight validation
    |
    v
capability check
    |
    v
protected local call
    |
    v
Service B
```

The goal is to approach function-call-like performance without removing isolation.

A same-layer call is conceptually expected to perform:

1. Caller/service identity validation
2. Capability and operation validation
3. Restricted entry-point validation
4. Fast permission/protection transition
5. Direct target invocation
6. Protection-state restoration

On ARM64, the research design explores POE/POE2 for fast permission overlays, PAC for control-flow hardening around protected entry points, and MTE for memory-safety detection inside service heaps and shared buffers. These mechanisms complement rather than replace capability authorization.

### Cross-Layer Service Calls — "The Stairs"

Cross-layer communication is used when a service needs another service located in a different layer under normal operating conditions.

The stair path is not intentionally slow. Any additional latency should result from the number of interactions, protection boundaries, or required coordination rather than from an artificially expensive implementation.

The layer hierarchy must not force unnecessary hops.

A service should be able to directly reach an authorized target in another layer through the shortest safe communication path.

### Elevator / VIP Fast Path

The Elevator is a protected fast path for critical or highly latency-sensitive communication. Its narrow transition stub is expected to use Assembly for the architecture-specific fast path and C for capability/policy validation.

Typical uses include:

- Direct communication with the Main Kernel
- Urgent kernel intervention
- Critical resource operations
- Sensitive control messages
- Fast communication with critical L1/L2 services
- Explicitly authorized emergency operations from L3/L4

Elevator access requires:

- Valid service identity
- Valid capability/token
- Explicit target permission
- Explicit operation permission
- Valid resource scope
- Restricted, explicitly approved entry point

The Elevator does not grant unrestricted access to the Main Kernel or an entire layer. PAC may be used to harden sensitive control-flow state around the gate, while authorization remains capability-based.

---

## Capability Model

Every service receives an explicit identity and a set of capabilities.

Conceptually:

```txt
Service Identity
      +
Capability
      +
Allowed Target
      +
Allowed Operation
      +
Allowed Resource
      =
Authorized Call
```

Example:

```txt
Camera Service:
    call Audio.capture            -> allowed
    submit frame to Display       -> allowed
    request buffer mapping        -> allowed
    modify scheduler internals    -> denied
    access arbitrary kernel RAM   -> denied
```

A service should receive only the minimum authority required to perform its role. Capabilities are narrow and operation-scoped; they are not general tokens that unlock a whole layer or the kernel.

---

## Isolation Model

Services remain isolated even when they are in the same layer.

Same-layer placement does not imply:

- Shared memory ownership
- Shared privilege
- Shared failure domain
- Unrestricted function calls
- Trust equivalence

The ARM64-first protection strategy is:

- **MMU/page-table protection** for the baseline isolation boundary
- **POE/POE2** as a forward-looking mechanism for fast permission overlays and same-address-space compartment transitions
- **PAC** for pointer/control-flow authentication around sensitive gates and entry points
- **MTE** for detecting classes of invalid memory access in service-local heaps and shared buffers
- **Separate protected stacks** where a call path crosses a trust or protection boundary
- **Restricted entry points** for protected service calls
- **Explicitly capability-authorized shared memory/buffers** for bulk data transfer

The roles are deliberately separated:

```txt
Authorization          = Capabilities
Baseline isolation     = MMU / page tables
Permission switching   = POE / POE2
Control-flow hardening = PAC
Memory-safety detection= MTE
Fast transition        = narrow ASM gate
```

MTE, PAC, and POE are complementary mechanisms; none of them replaces service authorization. POE/POE2 is treated as a forward-looking research mechanism and may be evaluated through supported hardware, emulation, or an abstracted prototype interface.

---

## Layer Placement Is Policy-Driven

The previous assignments represent the initial configuration of the architecture and are not permanent rules.

A component is assigned to a layer according to its properties rather than its name.

The placement policy considers:

- Criticality
- Trust level
- Latency sensitivity
- Communication frequency
- Communication affinity
- Required privileges
- Resource requirements
- Failure impact
- Code size
- Validation maturity
- Dependency relationships

Therefore, the same logical service may be assigned differently on different systems.

For example, a multimedia service that is non-critical on one device may become performance-critical on another platform and may therefore be placed closer to L2.

Similarly, a trusted native driver may be placed in L3, while a large third-party implementation of the same driver would normally remain in L4.

---

## Language Strategy

The initial language strategy is:

```txt
Main Kernel:
    Assembly + C
    Assembly only for architecture-specific fast paths, entry/exit, context/protection switching, and PAC/POE sequences
    C for minimal privileged control logic and capability enforcement

L1:
    Mainly C
    Assembly only for narrowly justified low-level or latency-critical operations

L2:
    Mainly C
    Rust may be used for non-ABI-critical support components where practical

L3:
    Prefer Rust for new native services
    C / C++ when required by libraries, hardware interfaces, or performance constraints

L4:
    Rust + C + C++
    Legacy and vendor code allowed when required, but strongly contained
```

The language does not define the security class.

The layer is determined by trust, criticality, resource requirements, latency, and communication behavior.

---

## ARM64-First Scope

The first research implementation targets ARM64.

The goal is not to build a complete production operating system. The prototype is intended to test the architectural mechanisms and their performance/isolation trade-offs.

### ARM64 protection assumptions

The design evaluates or prepares for the following Arm mechanisms:

- **PAC** — control-flow and pointer authentication for sensitive gates
- **MTE** — memory-safety error detection for tagged heaps and buffers
- **POE/POE2** — future/experimental permission overlays for fast compartment permission switching

The architecture should keep these mechanisms behind narrow architecture interfaces so that the high-level service model does not depend on one specific implementation detail.

The initial prototype should demonstrate only the mechanisms required to evaluate the architecture:

- Minimal Main/kernel-runtime core
- At least two service layers
- Isolated services
- Capability validation
- Same-layer protected calls
- Cross-layer communication
- Elevator fast path
- Restricted entry points
- A measurable protection-domain transition abstraction

A complete scheduler, POSIX environment, driver ecosystem, GUI stack, and full production hardware support are outside the first prototype.

Support for other instruction-set architectures is outside the initial scope.

---

## Relationship to Existing Work

The architecture should not claim that differentiated service treatment is entirely new.

Existing work already provides important related mechanisms:

- L4/seL4: minimal kernels, optimized IPC, capabilities, isolated services
- HongMeng: differentiated isolation classes and fast paths for performance-critical services
- Mixed-criticality systems: different scheduling and resource guarantees
- Compartmentalization systems (for example ERIM and CHERI-based work): lightweight protected execution domains and fast transitions; CHERI is treated as related work, not a required hardware dependency
- Capability-based systems: restricted authority and explicit access rights

The research question is whether these ideas can be organized into a unified layered service architecture that jointly considers:

- Criticality
- Trust
- Communication affinity
- Resource guarantees
- Latency
- Isolation
- Multiple communication paths

Novelty must be established through a complete related-work review rather than assumed.

---

## Open Research Questions

1. How should services be assigned to L1, L2, L3, and L4?
2. Can same-layer protected calls approach function-call performance while preserving isolation?
3. How much overhead does the Elevator introduce?
4. Can the Elevator remain secure under a compromised lower-layer service?
5. What is the cost of cross-layer communication?
6. Which mechanisms should be implemented in Assembly, C, or Rust?
7. How should MTE, PAC, and POE/POE2 be combined without conflating memory safety, control-flow integrity, permission switching, and authorization?
8. How should shared buffers be protected?
9. How should failures and service restarts be handled?
10. Does the layered design provide a measurable advantage over uniform isolation models?

---

## Recommended Prototype Order

The prototype should be implemented one mechanism at a time rather than as a full layered OS.

```txt
1. Service identity
2. Capability representation and validation
3. Restricted operation/entry-point table
4. Same-layer protected call skeleton
5. Cross-layer call skeleton
6. Elevator gate (C policy + narrow ARM64 Assembly stub)
7. Baseline MMU/page-based isolation
8. PAC hardening for sensitive gate/control-flow state
9. MTE-tagged service heaps/shared buffers
10. POE/POE2 permission-overlay experiment or abstraction
11. Microbenchmarks
12. Fault/isolation tests
```

The first implementation should not attempt to build all L1-L4 services. Synthetic services are sufficient to test the mechanisms.

### Initial benchmark set

Compare:

- Direct function call
- Same-layer protected call
- Cross-layer protected call
- Conventional process IPC baseline
- Elevator call

Measure initially:

- Median latency
- p95 / p99 latency
- CPU cycles per call
- Throughput
- Memory overhead

Add cache/TLB/context-switch counters only when they are needed to explain the primary results.

---

## Current Status

```txt
Problem definition        -> mostly defined; still being narrowed
Architecture concept      -> defined at high level; ARM64 protection strategy updated
Related work              -> in progress (seL4 MCS, HongMeng, ERIM, CHERI/compartmentalization)
Novelty                   -> not yet verified
ARM64 prototype           -> planning / first mechanisms next
Evaluation                -> not started
Paper                     -> notes/draft in progress
```
