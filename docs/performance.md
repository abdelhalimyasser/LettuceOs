# Performance and Reproducibility

## Evidence model

The recorded measurements live in [`results/raw/`](../results/raw/). The
original host CSVs were collected on the author's **Intel Core i5-1135G7**
development machine (`x86_64`); their provenance is recorded in
[`manifest.json`](../results/raw/host/manifest.json). ARM64 CSVs were captured
on QEMU `virt` using TCG. Processed CSVs in
[`results/processed/`](../results/processed/) are derivatives, not independent
measurements.

QEMU Generic Counter values are **emulator-relative ticks**, not physical ARM
CPU cycles or physical-silicon latency. They validate the exercised prototype
paths and permit comparisons within this captured run; physical ARM64
measurement remains future work. Physical ARM64 hardware was not available
for this evaluation.

GitHub Actions supplies separate hosted portability evidence on Ubuntu and
macOS x86_64/ARM64 runners. Those shared-runner observations are useful for
build and test reproducibility, but are not controlled ISA comparisons or
author-owned physical ARM measurements.

## Recorded host microbenchmarks

[`host-benchmarks.csv`](../results/raw/host/host-benchmarks.csv) records five
host-prototype paths using `CLOCK_MONOTONIC_RAW` on that local x86_64 system.
Its current p50 values are
2 ns for the direct baseline, 9 ns for capability checking, 38 ns for
Same-Layer, 35 ns for Cross-Layer, and 37 ns for Elevator. These are host
measurements, not ARM64 results or GitHub-hosted runner results.

The shared harness records 100 samples, 10,000 calls per sample, and 100,000
warm-up calls; see [`benchmark_common.h`](../benchmarks/benchmark_common.h).
The CSV is the authoritative source for percentiles and means.

## Scheduler evidence

[`scheduler-overhead.csv`](../results/raw/host/scheduler-overhead.csv) records
host decision-cost samples for Round Robin and EEVDF. RR has a simpler
selection path; EEVDF additionally evaluates eligibility and virtual deadlines.
That is a mechanism trade-off, not a claim that either policy is universally
faster.

[`scheduler-fairness.csv`](../results/raw/host/scheduler-fairness.csv) and
[`scheduler-controlled.txt`](../results/raw/host/scheduler-controlled.txt)
record the controlled fairness scenarios. Consult those files rather than
copying their rows into secondary tables.

## ARM64 prototype measurements

[`arm64-benchmarks.csv`](../results/raw/arm64/arm64-benchmarks.csv) contains
Cases A--K emitted by the QEMU run. In that capture, the Elevator C reference
(H) has p50 14,913 ticks and the specialized assembly mechanism (J) has p50
14,407 ticks. This is a 3.39% lower median in the captured emulated run, not a
physical-hardware speedup claim.

[`qemu-tests.csv`](../results/raw/arm64/qemu-tests.csv) contains 25 recorded
runtime test outcomes. QEMU execution supports architectural and mechanism
validation, but it does not model cache, pipeline, branch-predictor, or real
TLB timing faithfully enough for silicon latency claims.

## Re-running captures

`make bench` rewrites raw CSVs with a new run. Do not run it merely to inspect
the repository. For the current frozen evidence, inspect the tracked CSVs.
Use [`reproduce.md`](reproduce.md) for build and test commands.
