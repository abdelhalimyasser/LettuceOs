# POSIX-Lite Compatibility Layer

## 1. Scope & Design Philosophy

**Lettuce is a capability-based research microkernel, NOT a general-purpose POSIX operating system.** It deliberately avoids monolithic abstractions such as `fork()`, `exec()`, `signals`, and `mmap()`.

Instead, Lettuce provides **POSIX-lite**: a minimal, extensible, standards-compliant system call subset that allows standard runtime components, language libraries, and service runtimes to execute portably without rewriting basic I/O and time-query logic.

---

## 2. Implemented System Call Interface

System calls are invoked from EL0 via `SVC #5` with the syscall identifier passed in register `x8`:

| Syscall Number | Function Prototype | Description | Return Value |
|---|---|---|---|
| `1` (`LETTUCE_SYS_WRITE`) | `ssize_t write(int fd, const void *buf, size_t count)` | Writes bytes to an open descriptor | Bytes written or negative error |
| `2` (`LETTUCE_SYS_READ`) | `ssize_t read(int fd, void *buf, size_t count)` | Reads bytes from an open descriptor | Bytes read or negative error |
| `3` (`LETTUCE_SYS_CLOSE`) | `int close(int fd)` | Closes an open descriptor | `0` on success or negative error |
| `4` (`LETTUCE_SYS_GETPID`) | `pid_t getpid(void)` | Returns the calling task ID | `TaskId` |
| `5` (`LETTUCE_SYS_CLOCK_GETTIME`)| `int clock_gettime(clockid_t clk_id, struct timespec *tp)` | Queries the monotonic system clock | `0` on success or negative error |
| `6` (`LETTUCE_SYS_NANOSLEEP`) | `int nanosleep(const struct timespec *req, struct timespec *rem)` | Suspends execution until deadline | `0` on success or negative error |

---

## 3. Extensible File Descriptor Foundation

File descriptors are managed through an object-oriented operations-table structure in [`runtime/posix/include/fd.h`](../runtime/posix/include/fd.h):

```c
typedef struct LettuceFdOps {
    ssize_t (*read)(int fd, void *buf, size_t count);
    ssize_t (*write)(int fd, const void *buf, size_t count);
    int (*close)(int fd);
} LettuceFdOps;

typedef struct LettuceFileDescriptor {
    LettuceFdType type;
    uint32_t flags;
    const LettuceFdOps *ops;
    void *private_data;
} LettuceFileDescriptor;
```

### Pre-Installed Descriptors
Upon initialization (`lettuce_fd_table_init()`), slots `0`, `1`, and `2` are pre-wired:
- **`fd 0` (stdin):** `LETTUCE_FD_TYPE_CONSOLE_IN` (PL011 UART receiver).
- **`fd 1` (stdout):** `LETTUCE_FD_TYPE_CONSOLE_OUT` (PL011 UART transmitter).
- **`fd 2` (stderr):** `LETTUCE_FD_TYPE_CONSOLE_ERR` (PL011 UART transmitter).

---

## 4. Kernel Memory Boundary & Pointer Safety

The kernel validates all user pointers supplied in `SVC #5` calls before dereferencing:
1. **Null Pointer Check:** Null buffers with `count > 0` are rejected immediately with `-EFAULT`.
2. **Supervisor RAM Shield:** Pointers residing in the kernel's physical RAM region (`0x40000000 - 0x401fffff`) are rejected with `-EFAULT`. User-space tasks cannot trick `read()` or `write()` into reading from or overwriting kernel structures.
3. **Domain MMU Bounds:** User addresses must resolve within the active domain's private pages or the shared communication window; otherwise, hardware translation faults prevent unauthorized access.

---

## 5. Extensibility: Adding Future Subsystems

The POSIX-lite layer provides a template for future non-monolithic extensions:
- **Virtual File Systems / Pipes:** New subsystems implement a `LettuceFdOps` struct and install into slots `3..15`.
- **IPC Sockets:** Network or local IPC endpoints can register socket operations tables without altering the core microkernel ABI.
- **Pthreads / Scheduling:** Future user-level threading models can interface directly with `lettuce_task_create()` and `SVC #4` (yield) while consuming standard time APIs.
