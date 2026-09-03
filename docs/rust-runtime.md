# Rust Runtime

[`runtime/rust/`](../runtime/rust/) is an EL0-facing wrapper crate over the C
ABI. It provides typed `CapabilityHandle`, `TaskId`, `ServiceId`, status,
shared-buffer, and POSIX-lite interfaces.

Unsafe FFI is localized to the small `extern "C"` boundaries in the modules.
The Rust code wraps C runtime and syscall entry points; it does not implement
EL1 dispatch, MMU switching, capability policy, or exception vectors.

`posix.rs` exposes wrappers for write, read, close, PID, monotonic time, and
sleep. `service.rs` currently exposes Same-Layer calls. `buffer.rs` accesses
the C shared-buffer API. These wrappers preserve the C ABI's authorization and
ownership rules rather than replacing them.
