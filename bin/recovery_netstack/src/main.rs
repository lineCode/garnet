// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! A networking stack.

#![feature(const_fn)]
#![feature(nonzero)]
// In case we roll the toolchain and something we're using as a feature has been
// stabilized.
#![allow(stable_features)]
// We use repr(packed) in some places (particularly in the wire module) to
// create structs whose layout matches the layout of network packets on the
// wire. This ensures that the compiler will stop us from using repr(packed) in
// an unsound manner without using unsafe code.
#![deny(safe_packed_borrows)]
#![deny(missing_docs)]

extern crate byteorder;
#[macro_use]
extern crate failure;
#[cfg(test)]
extern crate rand;
extern crate zerocopy;

// mark all modules as public so that deny(missing_docs) will be more powerful
pub mod device;
pub mod error;
pub mod ip;
#[cfg(test)]
pub mod testutil;
pub mod transport;
pub mod wire;

fn main() {}
