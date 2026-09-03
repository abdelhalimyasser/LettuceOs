/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: runtime/rust/src/task.rs
 *
 * Purpose:
 *   Provides typed Rust task identifiers and task-related C ABI wrappers.
 *
 * Design:
 *   Unsafe FFI is localized here; task lifecycle remains kernel controlled.
 */

#[repr(transparent)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct TaskId(pub u32);

impl TaskId {
	pub const fn new(id: u32) -> Self {
		Self(id)
	}

	pub const fn raw(self) -> u32 {
		self.0
	}

	pub const fn slot(self) -> u16 {
		(self.0 & 0xffff) as u16
	}

	pub const fn generation(self) -> u16 {
		(self.0 >> 16) as u16
	}
}
