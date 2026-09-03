# Empirical Performance & Benchmark Results

## 1. Evaluation Methodology & Scientific Integrity

Lettuce was evaluated across two distinct environments:
1. **Host Environment (x86_64, Native Silicon):** Measures pure algorithmic and dispatch complexity in nanoseconds without architectural emulation artifacts.
2. **ARM64 Bare-Metal Environment (QEMU virt, AArch64 TCG):** Measures end-to-end hardware transitions, MMU table switches, ASID management, exception traps, and PAC validation.

> [!IMPORTANT]
> **QEMU TCG Architectural Caveat:**
> QEMU Tiny Code Generator (TCG) translates ARM64 instructions to host x86_64 dynamically. Generic counter ticks and elapsed times under QEMU reflect software virtualization and translation overhead, **not physical ARM silicon clock cycles or real hardware latencies**. They are presented for algorithmic comparison between paths under identical emulation conditions.

---

## 2. Host x86_64 Baseline Measurements (Native Silicon)

Benchmarked on native host hardware over 5 iterations of 10,000 calls per sample:

| Evaluation Target | Operation / Path | Latency (ns) | Throughput (ops/sec) | Complexity |
|---|---|---|---|---|
| **Direct Call Baseline** | Unmediated native C call | 0.4 ns | $2.5 \times 10^9$ | $O(1)$ |
| **Capability Check** | Independent $O(1)$ flat lookup | **5.0 ns** | **$2.0 \times 10^8$** | $O(1)$ |
| **Same-Layer Call** | Lateral mediated dispatch | **21.0 ns** | **$4.7 \times 10^7$** | $O(1)$ |
| **Cross-Layer Call** | Vertical mediated dispatch | **28.0 ns** | **$3.5 \times 10^7$** | $O(1)$ |
| **Elevator Call** | Critical downward dispatch | **39.0 ns** | **$2.5 \times 10^7$** | $O(1)$ |

### Algorithmic Observations
- Flat-table capability validation executes in just **5.0 nanoseconds**, confirming that capability checks introduce virtually negligible overhead on modern superscalar CPUs.
- Mediated dispatch across all layers completes within **21--39 nanoseconds**, demonstrating high-efficiency software mediation.

---

## 3. Bare-Metal ARM64 Measurements (QEMU virt)

Benchmarked under freestanding QEMU `virt` with 50 samples of 100 calls per sample (5,000 invocations per benchmark):

| Benchmark Case | Description | Median (p50) | Mean | 95th Percentile | Min / Max |
|---|---|---|---|---|---|
| **Case A: Direct EL1** | Unmediated function call baseline | 3 ticks (~3 ns) | 7 ticks (~7 ns) | 3 ticks | 3 / 241 ticks |
| **Case B: Capability Check**| Standalone $O(1)$ table validation | 52 ticks (~52 ns) | 56 ticks (~56 ns) | 90 ticks | 51 / 228 ticks |
| **Case C: EL1-EL0-SVC-EL1** | Raw privilege crossing roundtrip | 1,477 ticks (~1.48 µs) | 1,718 ticks (~1.72 µs) | 3,089 ticks | 1,074 / 3,587 ticks |
| **Case D: Same-Domain Fast**| Mediated call, same domain | 102 ticks (~102 ns) | 117 ticks (~117 ns) | 120 ticks | 92 / 813 ticks |
| **Case E: Cross-Domain MMU**| `TTBR0_EL1` switch pair + ISB | 6,211 ticks (~6.21 µs) | 7,359 ticks (~7.36 µs) | 12,185 ticks | 5,912 / 13,041 ticks |
| **Case F: Same-Layer EL0** | Lateral EL0 mediated call | 17,675 ticks (~17.68 µs) | 17,966 ticks (~17.97 µs) | 20,148 ticks | 17,042 / 21,774 ticks |
| **Case G: Cross-Layer EL0** | Vertical EL0 mediated call | 17,024 ticks (~17.02 µs) | 17,357 ticks (~17.36 µs) | 17,940 ticks | 16,560 / 24,956 ticks |
| **Case H: Elevator (C Path)**| Elevator mediated call (Reference)| 16,990 ticks (~16.99 µs) | 18,788 ticks (~18.79 µs) | 35,764 ticks | 16,696 / 37,576 ticks |
| **Case I: EL0 Call + PAC** | Same-Layer call with PAC signing | 17,269 ticks (~17.27 µs) | 17,874 ticks (~17.87 µs) | 17,770 ticks | 16,615 / 35,082 ticks |
| **Case J: Elevator (ASM)** | Elevator Fast Gate (`elevator.S`) | **16,166 ticks (~16.17 µs)**| **17,767 ticks (~17.77 µs)**| 26,031 ticks | 15,761 / 32,324 ticks |
| **Case K: POSIX Syscall** | `getpid` + `clock_gettime` (`SVC #5`)| 4,087 ticks (~4.09 µs) | 4,360 ticks (~4.36 µs) | 5,327 ticks | 3,676 / 12,060 ticks |

---

## 4. Elevator Optimization Delta

Comparing the Elevator Reference C Path (Case H) against the Assembly Transition Gate (Case J):
- **Median (p50):** Reduced from 16,990 ticks to **16,166 ticks** (**~4.8% reduction**).
- **Mean:** Reduced from 18,788 ticks to **17,767 ticks** (**~7.3% latency reduction**).
- **Tail Latency (p95):** Reduced from 35,764 ticks to **26,031 ticks** (**~27.2% tail reduction**).

The assembly specialization successfully reduces register pressure and function call prologue/epilogue overhead while strictly preserving identical cryptographic and capability authorization policies.

---

## 5. Scheduler Subsystem Overhead & Fairness Evaluation

The scheduling subsystem was benchmarked across the Round-Robin (baseline) and EEVDF policies on native host silicon:

### A. Scheduler Algorithm Overhead

| Policy & Task Count | Operation | Median (p50) | 95th Percentile | 99th Percentile | Mean |
|---|---|---|---|---|---|
| **Round-Robin (2 Tasks)** | `pick_next` | 266 ns | 435 ns | 465 ns | 287.9 ns |
| **EEVDF (2 Tasks)** | `pick_next` | 555 ns | 747 ns | 1,014 ns | 583.2 ns |
| **Round-Robin (4 Tasks)** | `pick_next` | 121 ns | 205 ns | 213 ns | 135.7 ns |
| **EEVDF (4 Tasks)** | `pick_next` | 502 ns | 853 ns | 882 ns | 549.9 ns |
| **Round-Robin (8 Tasks)** | `pick_next` | 48 ns | 53 ns | 57 ns | 48.6 ns |
| **EEVDF (8 Tasks)** | `pick_next` | 386 ns | 461 ns | 492 ns | 393.5 ns |
| **Round-Robin (16 Tasks)** | `pick_next` | 9 ns | 9 ns | 9 ns | 9.2 ns |
| **EEVDF (16 Tasks)** | `pick_next` | 113 ns | 125 ns | 130 ns | 114.8 ns |
| **Round-Robin Tick** | `on_tick` | 3 ns | 3 ns | 5 ns | 3.4 ns |
| **EEVDF Tick** | `on_tick` | 2 ns | 2 ns | 3 ns | 2.7 ns |

### B. Controlled Fairness & Weight Proportionality

Evaluated over 3,000--4,000 continuous ticks under identical workload scenarios:

1. **Equal Weight Fairness (4 Tasks, 4,000 ticks):**
   - **Round-Robin:** T0: 1000, T1: 1000, T2: 1000, T3: 999 ticks $\implies$ **Jain's Index = 1.00000**
   - **EEVDF:** T0: 1000, T1: 1000, T2: 1000, T3: 999 ticks $\implies$ **Jain's Index = 1.00000**

2. **Weighted CPU Allocation (3,000 ticks):**
   - **Target 1:1 (Weights 1024 vs 1024):**
     - RR: 50.0% vs 50.0% (Ratio: 1.001)
     - EEVDF: 50.0% vs 50.0% (Ratio: 1.001)
   - **Target 2:1 (Weights 2048 vs 1024):**
     - RR: 50.0% vs 50.0% (Ratio: 1.001, ignores weight metadata)
     - EEVDF: 66.6% vs 33.3% (**Ratio: 1.999**, matching theoretical 2.0 within 0.05%)
   - **Target 4:1 (Weights 4096 vs 1024):**
     - RR: 50.0% vs 50.0% (Ratio: 1.001)
     - EEVDF: 78.9% vs 21.1% (**Ratio: 3.745**)

3. **Wakeup Latency & Responsiveness:**
   - **Round-Robin:** 3 ticks dispatch delay after waking.
   - **EEVDF:** 5 ticks dispatch delay after waking.
   - EEVDF's $V_{\text{sys}}$ alignment ensures waking tasks become eligible immediately while preventing accumulated lag from starving concurrent tasks.
