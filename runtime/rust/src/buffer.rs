// SPDX-License-Identifier: Apache-2.0

use crate::capability::CapabilityHandle;

#[repr(transparent)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct BufferHandle(pub u32);

unsafe extern "C" {
	fn lettuce_shared_buffer_access(handle: u32, capability: u32, write: bool, data: *mut *mut u8, size: *mut usize) -> crate::service::Status;
}

pub struct Buffer {
	pub handle: BufferHandle,
}

impl Buffer {
	pub fn access(&self, capability: CapabilityHandle, write: bool) -> crate::service::Status {
		let mut data = core::ptr::null_mut();
		let mut size = 0usize;
		unsafe { lettuce_shared_buffer_access(self.handle.0, capability.raw(), write, &mut data, &mut size) }
	}
}
