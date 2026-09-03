# Rust Runtime Integration & Language Policy

## 1. Operating System Language Policy

Lettuce enforces a disciplined language tiering model across the operating system stack:

```text
 ┌─────────────────────────────────────────────────────────┐
 │ Tier 3: High-Level Services & Application Runtimes      │
 │ Language: Safe Rust                                     │
 │ Purpose: Type safety, borrow checker, ergonomic APIs    │
 ├─────────────────────────────────────────────────────────┤
 │ Tier 2: Kernel Microkernel Core & Policies             │
 │ Language: C (C99 / C11)                                 │
 │ Purpose: Predictable control, deterministic MMU & GIC,  │
 │          zero dynamic allocation, stable ABI            │
 ├─────────────────────────────────────────────────────────┤
 │ Tier 1: Hardware-Specific Architectural Primitives      │
 │ Language: ARM64 Assembly                                │
 │ Purpose: Exception vectors, DAIF manipulation, context  │
 │          swapping, Elevator transition gate, PAC keys   │
 └─────────────────────────────────────────────────────────┘
```

### C++ Policy
C++ is **not** utilized inside the Lettuce kernel core to avoid hidden heap allocations, complex exception runtimes, and non-deterministic overheads. C++ user-space components are supported externally by linking against the clean C ABI headers.

---

## 2. Rust Runtime Scope & Responsibilities

The Rust runtime package ([`runtime/rust/`](../runtime/rust/)) provides memory-safe abstractions for services executing at EL0:
- **Service SDK:** Encapsulates service identity, registration, and dispatch results.
- **Type-Safe Capabilities:** Wraps raw 32-bit capability handles in strongly typed structures preventing bitmask corruption.
- **Task Identification:** Unpacks 32-bit generational task handles into slot indices and generation counters.
- **POSIX-Lite Bindings:** Wraps raw system calls in idiomatic `Result<T, i32>` types with automatic `errno` translation.

---

## 3. Module Overview

### `runtime/rust/src/posix.rs`
Provides safe wrappers around `SVC #5` calls:
```rust
pub fn stdout_write(s: &str) -> Result<usize, i32>;
pub fn stderr_write(s: &str) -> Result<usize, i32>;
pub fn stdin_read(buf: &mut [u8]) -> Result<usize, i32>;
pub fn close_fd(fd: i32) -> Result<(), i32>;
pub fn monotonic_time() -> Result<Timespec, i32>;
pub fn sleep_ms(ms: u64) -> Result<(), i32>;
pub fn getpid() -> u32;
```

### `runtime/rust/src/task.rs`
Provides generational task handle manipulation:
```rust
#[repr(transparent)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct TaskId(pub u32);

impl TaskId {
    pub const fn raw(self) -> u32 { self.0 }
    pub const fn slot(self) -> u16 { (self.0 & 0xffff) as u16 }
    pub const fn generation(self) -> u16 { (self.0 >> 16) as u16 }
}
```

### `runtime/rust/src/capability.rs`
Enforces capability handle semantics at compile time:
```rust
#[repr(transparent)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct CapabilityHandle(pub u32);
```

---

## 4. Zero-Allocation & Unsafe Boundary Guarantees

1. **No Global Allocator:** The Rust runtime requires neither a global heap allocator (`#[global_allocator]`) nor the `alloc` crate.
2. **Stack & Borrow Semantics:** All operations work on borrowed slices (`&str`, `&[u8]`, `&mut [u8]`) or small stack-allocated primitives.
3. **Localized Unsafe Blocks:** Unsafe code is strictly quarantined to the immediate FFI wrapper boundary invoking system calls or C runtime interfaces.
