// Copyright 2015 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_UI_VIEW_FRAMEWORK_VIEW_PROVIDER_SERVICE_H_
#define LIB_UI_VIEW_FRAMEWORK_VIEW_PROVIDER_SERVICE_H_

#include <functional>
#include <vector>

#include <fuchsia/ui/views_v1/cpp/fidl.h>
#include "lib/app/cpp/startup_context.h"
#include "lib/fidl/cpp/binding_set.h"
#include "lib/fxl/macros.h"
#include "lib/ui/view_framework/base_view.h"

namespace mozart {

// Parameters for creating a view.
struct ViewContext {
  fuchsia::sys::StartupContext* startup_context;
  ::fuchsia::ui::views_v1::ViewManagerPtr view_manager;
  fidl::InterfaceRequest<::fuchsia::ui::views_v1_token::ViewOwner>
      view_owner_request;
  fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> outgoing_services;
};

// A callback to create a view in response to a call to
// |ViewProvider.CreateView()|.
using ViewFactory =
    std::function<std::unique_ptr<BaseView>(ViewContext context)>;

// Publishes a view provider as an outgoing service of the application.
// The views created by the view provider are owned by it and will be destroyed
// when the view provider itself is destroyed.
//
// This is only intended to be used for simple example programs.
class ViewProviderService : public ::fuchsia::ui::views_v1::ViewProvider {
 public:
  explicit ViewProviderService(fuchsia::sys::StartupContext* startup_context,
                               ViewFactory factory);
  ~ViewProviderService();

  // |ViewProvider|
  void CreateView(
      fidl::InterfaceRequest<::fuchsia::ui::views_v1_token::ViewOwner>
          view_owner_request,
      fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> view_services)
      override;

 private:
  fuchsia::sys::StartupContext* startup_context_;
  ViewFactory view_factory_;

  fidl::BindingSet<ViewProvider> bindings_;
  std::vector<std::unique_ptr<BaseView>> views_;

  FXL_DISALLOW_COPY_AND_ASSIGN(ViewProviderService);
};

}  // namespace mozart

#endif  // LIB_UI_VIEW_FRAMEWORK_VIEW_PROVIDER_SERVICE_H_
