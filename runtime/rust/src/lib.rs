/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: runtime/rust/src/lib.rs
 *
 * Purpose:
 *   Assembles the safe Rust service-runtime modules exposed to EL0 clients.
 *
 * Design:
 *   This crate wraps existing C ABI contracts and does not implement privileged
 *   kernel mechanisms.
 */

pub mod service;
pub mod capability;
pub mod buffer;
pub mod posix;
pub mod task;

pub use capability::CapabilityHandle;
pub use service::{Service, ServiceId, Status};
pub use task::TaskId;
