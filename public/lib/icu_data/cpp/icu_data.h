// Copyright 2015 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_ICU_DATA_CPP_ICU_DATA_H_
#define LIB_ICU_DATA_CPP_ICU_DATA_H_

namespace fuchsia {
namespace sys {
class StartupContext;
}
}

namespace icu_data {

bool Initialize(fuchsia::sys::StartupContext* context);
bool Release();

}  // namespace icu_data

#endif  // LIB_ICU_DATA_CPP_ICU_DATA_H_
