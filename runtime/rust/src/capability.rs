// SPDX-License-Identifier: Apache-2.0

#[repr(transparent)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct CapabilityHandle(u32);

impl CapabilityHandle {
	pub const INVALID: Self = Self(0);
	pub const fn from_raw(value: u32) -> Self { Self(value) }
	pub const fn raw(self) -> u32 { self.0 }
}
