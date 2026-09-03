/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: runtime/rust/src/posix.rs
 *
 * Purpose:
 *   Provides typed Rust wrappers around the Lettuce POSIX-lite C ABI.
 *
 * Flow:
 *   Safe Rust API -> C ABI FFI -> EL0 C runtime -> SVC/syscall path.
 *
 * This file does not:
 *   Implement privileged kernel syscalls.
 */

#[repr(C)]
#[derive(Debug, Clone, Copy, Default)]
pub struct Timespec {
	pub tv_sec: i64,
	pub tv_nsec: i64,
}

unsafe extern "C" {
	fn lettuce_write(fd: i32, buf: *const u8, count: usize) -> isize;
	fn lettuce_read(fd: i32, buf: *mut u8, count: usize) -> isize;
	fn lettuce_close(fd: i32) -> i32;
	fn lettuce_getpid() -> i32;
	fn lettuce_clock_gettime(clk_id: i32, tp: *mut Timespec) -> i32;
	fn lettuce_nanosleep(req: *const Timespec, rem: *mut Timespec) -> i32;
	fn lettuce_errno_location() -> *mut i32;
}

pub const CLOCK_MONOTONIC: i32 = 1;

pub fn errno() -> i32 {
	unsafe { *lettuce_errno_location() }
}

pub fn stdout_write(s: &str) -> Result<usize, i32> {
	let bytes = s.as_bytes();
	let res = unsafe { lettuce_write(1, bytes.as_ptr(), bytes.len()) };
	if res < 0 {
		Err(errno())
	} else {
		Ok(res as usize)
	}
}

pub fn stdin_read(buf: &mut [u8]) -> Result<usize, i32> {
	let res = unsafe { lettuce_read(0, buf.as_mut_ptr(), buf.len()) };
	if res < 0 {
		Err(errno())
	} else {
		Ok(res as usize)
	}
}

pub fn close_fd(fd: i32) -> Result<(), i32> {
	let res = unsafe { lettuce_close(fd) };
	if res != 0 {
		Err(errno())
	} else {
		Ok(())
	}
}

pub fn stderr_write(s: &str) -> Result<usize, i32> {
	let bytes = s.as_bytes();
	let res = unsafe { lettuce_write(2, bytes.as_ptr(), bytes.len()) };
	if res < 0 {
		Err(errno())
	} else {
		Ok(res as usize)
	}
}

pub fn monotonic_time() -> Result<Timespec, i32> {
	let mut ts = Timespec::default();
	let res = unsafe { lettuce_clock_gettime(CLOCK_MONOTONIC, &mut ts) };
	if res != 0 {
		Err(errno())
	} else {
		Ok(ts)
	}
}

pub fn sleep_ms(ms: u64) -> Result<(), i32> {
	let req = Timespec {
		tv_sec: (ms / 1000) as i64,
		tv_nsec: ((ms % 1000) * 1_000_000) as i64,
	};
	let res = unsafe { lettuce_nanosleep(&req, core::ptr::null_mut()) };
	if res != 0 {
		Err(errno())
	} else {
		Ok(())
	}
}

pub fn getpid() -> u32 {
	unsafe { lettuce_getpid() as u32 }
}
