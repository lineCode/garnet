// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GARNET_BIN_APPMGR_ROOT_LOADER_H_
#define GARNET_BIN_APPMGR_ROOT_LOADER_H_

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/zx/vmo.h>

#include "lib/fidl/cpp/binding_set.h"
#include "lib/fxl/macros.h"

namespace fuchsia {
namespace sys {

class RootLoader : public Loader {
 public:
  explicit RootLoader();
  ~RootLoader() override;

  void LoadComponent(fidl::StringPtr url,
                     LoadComponentCallback callback) override;

  void AddBinding(fidl::InterfaceRequest<Loader> request);

 private:
  fidl::BindingSet<Loader> bindings_;

  FXL_DISALLOW_COPY_AND_ASSIGN(RootLoader);
};

}  // namespace sys
}  // namespace fuchsia

#endif  // GARNET_BIN_APPMGR_ROOT_LOADER_H_
