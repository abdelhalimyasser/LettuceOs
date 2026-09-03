/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: runtime/rust/src/service.rs
 *
 * Purpose:
 *   Defines Rust service-facing types and wrappers for the Lettuce C ABI.
 *
 * Design:
 *   The module keeps typed service interactions in EL0-facing code while the
 *   supervisor retains registry, dispatch, and authorization ownership.
 */

use crate::capability::CapabilityHandle;

#[repr(transparent)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct Status(pub u32);

#[repr(transparent)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct ServiceId(pub u32);

unsafe extern "C" {
	fn lettuce_same_layer_call(target: u32, operation: u32, resource: u32, capability: u32) -> Status;
}

pub struct Service {
	pub id: ServiceId,
}

impl Service {
	pub fn same_layer_call(&self, target: ServiceId, operation: u32, resource: u32, capability: CapabilityHandle) -> Status {
		unsafe { lettuce_same_layer_call(target.0, operation, resource, capability.raw()) }
	}
}
