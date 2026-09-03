# Interrupts and Scheduling

```text
Generic Timer -> GICv2 acknowledge -> EL1 IRQ handler -> scheduler mechanism
              -> policy pick_next -> context/domain handoff -> EOI -> ERET
```

[`gic.c`](../kernel/arch/arm64/gic.c) implements GICv2 setup,
acknowledgement, and end-of-interrupt. [`timer.c`](../kernel/arch/arm64/timer.c)
programs the Generic Timer. IRQ handlers in
[`kernel/arch/arm64/cpu.c`](../kernel/arch/arm64/cpu.c) acknowledge the timer,
run its handler, and issue EOI for non-spurious interrupts.

The common mechanism in [`kernel/scheduler/scheduler.c`](../kernel/scheduler/scheduler.c)
owns task lifecycle, sleeping/waking, context switching, and domain handoff.
Policies implement the hooks in [`policy.h`](../kernel/scheduler/policy.h):
they account for time and select ready tasks, but do not manipulate exception
vectors or MMU state.

Round Robin in [`rr.c`](../kernel/scheduler/rr.c) performs a bounded rotating
ready-task selection. EEVDF in [`eevdf.c`](../kernel/scheduler/eevdf.c) uses
integer fixed-point virtual time, eligibility, weights, and virtual deadlines.
RR is simpler; EEVDF supports weighted proportional selection. The scheduler
uses statically bounded task state and avoids heap allocation in its hot paths.

Current host scheduler evidence is in
[`results/raw/host/scheduler-overhead.csv`](../results/raw/host/scheduler-overhead.csv)
and [`scheduler-fairness.csv`](../results/raw/host/scheduler-fairness.csv).
Those recorded values should be used instead of historical copied tables.
