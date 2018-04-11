// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#if defined(__Fuchsia__)
#include "garnet/lib/debug_ipc/helper/message_loop_zircon.h"
#endif

namespace debug_ipc {

#if defined(__Fuchsia__)
using PlatformMessageLoop = MessageLoopZircon;
#else
#error Need a host implementation.
#endif

}  // namespace