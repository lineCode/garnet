// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FIDL_ENABLE_LEGACY_WAIT_FOR_RESPONSE

#include <fidl/test/compatibility/cpp/fidl.h>
#include <lib/async-loop/cpp/loop.h>
#include <zx/channel.h>
#include <cstdlib>
#include <string>

#include "garnet/public/lib/fidl/compatibility_test/echo_client_app.h"
#include "lib/app/cpp/startup_context.h"
#include "lib/fidl/cpp/binding_set.h"
#include "lib/fidl/cpp/interface_request.h"

namespace fidl {
namespace test {
namespace compatibility {

class EchoServerApp : public Echo {
 public:
  EchoServerApp()
      : context_(fuchsia::sys::StartupContext::CreateFromStartupInfo()) {
    context_->outgoing().AddPublicService<Echo>(
        [this](fidl::InterfaceRequest<Echo> request) {
          bindings_.AddBinding(
              this, fidl::InterfaceRequest<Echo>(std::move(request)));
        });
  }

  ~EchoServerApp() {}

  void EchoStruct(Struct value, fidl::StringPtr forward_to_server,
                  EchoStructCallback callback) override {
    if (!forward_to_server->empty()) {
      EchoClientApp app;
      app.Start(forward_to_server.get());
      app.echo()->EchoStruct(std::move(value), "", std::move(callback));
      const zx_status_t wait_status = app.echo().WaitForResponse();
      if (wait_status != ZX_OK) {
        fprintf(stderr, "Proxy Got error %d waiting for response from %s\n",
                wait_status, forward_to_server->c_str());
      }
    } else {
      callback(std::move(value));
    }
  }

  void EchoStructNoRetVal(Struct value,
                          fidl::StringPtr forward_to_server) override {
    if (!forward_to_server->empty()) {
      std::unique_ptr<EchoClientApp> app(new EchoClientApp);
      app->Start(forward_to_server.get());
      app->echo().events().EchoEvent = [this](Struct resp) {
        this->HandleEchoEvent(std::move(resp));
      };
      app->echo()->EchoStructNoRetVal(std::move(value), "");
      client_apps_.push_back(std::move(app));
    } else {
      for (const auto& binding : bindings_.bindings()) {
        Struct to_send;
        value.Clone(&to_send);
        binding->events().EchoEvent(std::move(to_send));
      }
    }
  }

 private:
  void HandleEchoEvent(Struct value) {
    for (const auto& binding : bindings_.bindings()) {
      Struct to_send;
      value.Clone(&to_send);
      binding->events().EchoEvent(std::move(to_send));
    }
  }

  EchoPtr server_ptr;
  EchoServerApp(const EchoServerApp&) = delete;
  EchoServerApp& operator=(const EchoServerApp&) = delete;

  std::unique_ptr<fuchsia::sys::StartupContext> context_;
  fidl::BindingSet<Echo> bindings_;
  std::vector<std::unique_ptr<EchoClientApp>> client_apps_;
};

}  // namespace compatibility
}  // namespace test
}  // namespace fidl

int main(int argc, const char** argv) {
  // The FIDL support lib requires async_get_default() to return non-null.
  async::Loop loop(&kAsyncLoopConfigMakeDefault);

  fidl::test::compatibility::EchoServerApp app;
  loop.Run();
  return EXIT_SUCCESS;
}
