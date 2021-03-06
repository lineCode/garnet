// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <trace-provider/provider.h>

#include "garnet/examples/ui/shapes_a11y/shapes_a11y_view.h"
#include "lib/fsl/tasks/message_loop.h"
#include "lib/ui/view_framework/view_provider_app.h"

int main(int argc, const char** argv) {
  fsl::MessageLoop loop;
  trace::TraceProvider trace_provider(loop.async());
  // Launching server app

  // Initializing view
  mozart::ViewProviderApp app([](mozart::ViewContext view_context) {
    return std::make_unique<examples::ShapesA11yView>(
        std::move(view_context.view_manager),
        std::move(view_context.view_owner_request));
  });
  loop.Run();
  return 0;
}
