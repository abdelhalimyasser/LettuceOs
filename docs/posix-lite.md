# POSIX-Lite Interface

Lettuce implements a small POSIX-like ABI, not a complete POSIX operating
system. There is no `fork`, `exec`, signal subsystem, virtual file system, or
general `mmap` interface.

```text
EL0 C/Rust wrapper -> SVC #5 -> EL1 dispatcher -> syscall service -> ERET
```

`SVC #5` is the immediate used to enter the syscall dispatcher. The syscall
number is separately carried in `x8`; arguments use `x0`, `x1`, and `x2` as
required by each call. The implemented identifiers in
[`kernel/include/arch.h`](../kernel/include/arch.h) are `write`, `read`,
`close`, `getpid`, `clock_gettime`, and `nanosleep`.

The EL0 wrappers are in [`runtime/posix/src/posix.c`](../runtime/posix/src/posix.c).
EL1 services are in [`runtime/posix/src/sys.c`](../runtime/posix/src/sys.c),
and the bounded 16-slot descriptor table is in
[`runtime/posix/src/fd.c`](../runtime/posix/src/fd.c). Descriptors 0--2 are
console endpoints.

`CLOCK_MONOTONIC` uses the ARM Generic Counter on ARM64. `nanosleep` marks the
current task sleeping until a scheduler timer deadline. Pointer checks reject
null buffers where applicable and supervisor RAM addresses; this prototype is
not a general copy-in/copy-out subsystem, so callers must not infer complete
POSIX memory-safety semantics from these checks.
